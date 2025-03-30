#define log_fmt(fmt) "acpi-ec: " fmt
#include <arch/io.h>
#include <arch/pci.h>
#include <arch/acpi.h>

#include <kernel/time.h>
#include <kernel/kernel.h>
#include <kernel/compiler.h>
#include <kernel/spinlock.h>

#include <uacpi/io.h>
#include <uacpi/acpi.h>
#include <uacpi/event.h>
#include <uacpi/sleep.h>
#include <uacpi/uacpi.h>
#include <uacpi/status.h>
#include <uacpi/opregion.h>
#include <uacpi/resources.h>
#include <uacpi/utilities.h>

typedef struct acpi_gas acpi_gas_t;

enum
{
    OBF     = (1 << 0),
    IBF     = (1 << 1),
    BURST   = (1 << 4),
    SCI_EVT = (1 << 5),

    RD_EC     = 0x80,
    WR_EC     = 0x81,
    BE_EC     = 0x82,
    BD_EC     = 0x83,
    QR_EC     = 0x84,
    BURST_ACK = 0x90,
};

struct acpi_ec
{
    uacpi_namespace_node* node;
    spinlock_t            spinlock;
    acpi_gas_t            control;
    acpi_gas_t            data;
};

typedef struct acpi_ec acpi_ec_t;

static acpi_ec_t acpi_ec = {
    .spinlock = SPINLOCK_INIT()
};

#define WAIT_WHILE(condition) \
    ({ \
        int timeout = 5000; \
        while ((condition) && --timeout) \
        { \
            udelay(100); \
        } \
        !timeout; \
    })

static uint8_t acpi_ec_readb_impl(struct acpi_gas* gas)
{
    uint64_t val;
    uacpi_gas_read(gas, &val);
    return val;
}

static void acpi_ec_writeb_impl(struct acpi_gas* gas, uint8_t value)
{
    uacpi_gas_write(gas, value);
}

static void acpi_ec_ibf_poll(void)
{
    if (WAIT_WHILE(acpi_ec_readb_impl(&acpi_ec.control) & IBF))
    {
        log_warning("IBF polling timeout");
    }
}

static void acpi_ec_obf_poll(void)
{
    if (WAIT_WHILE(!(acpi_ec_readb_impl(&acpi_ec.control) & OBF)))
    {
        log_warning("OBF polling timeout");
    }
}

static uint8_t acpi_ec_reg_readb(struct acpi_gas* gas)
{
    acpi_ec_obf_poll();
    return acpi_ec_readb_impl(gas);
}

static void acpi_ec_reg_writeb(struct acpi_gas* gas, uint8_t value)
{
    acpi_ec_ibf_poll();
    return acpi_ec_writeb_impl(gas, value);
}

static uint8_t acpi_ec_read(uint8_t offset)
{
    acpi_ec_reg_writeb(&acpi_ec.control, RD_EC);
    acpi_ec_reg_writeb(&acpi_ec.data, offset);
    return acpi_ec_reg_readb(&acpi_ec.data);
}

static void acpi_ec_write(uint8_t value, uint8_t offset)
{
    acpi_ec_reg_writeb(&acpi_ec.control, WR_EC);
    acpi_ec_reg_writeb(&acpi_ec.data, offset);
    acpi_ec_reg_writeb(&acpi_ec.data, value);
}

static int acpi_ec_burst_enable(void)
{
    acpi_ec_reg_writeb(&acpi_ec.control, BE_EC);
    uint8_t data = acpi_ec_reg_readb(&acpi_ec.data);
    if (unlikely(data != BURST_ACK))
    {
        return -EIO;
    }
    return 0;
}

static int acpi_ec_burst_disable(void)
{
    acpi_ec_reg_writeb(&acpi_ec.control, BD_EC);
    if (WAIT_WHILE(acpi_ec_readb_impl(&acpi_ec.control) & BURST))
    {
        return -ETIMEDOUT;
    }
    return 0;
}

static uacpi_status acpi_ec_rw(uacpi_region_op op, uacpi_region_rw_data* data)
{
    if (unlikely(data->byte_width != 1))
    {
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    scoped_spinlock_irq_lock(&acpi_ec.spinlock);

    acpi_ec_burst_enable();

    switch (op)
    {
        case UACPI_REGION_OP_READ:
            data->value = acpi_ec_read(data->offset);
            break;
        case UACPI_REGION_OP_WRITE:
            acpi_ec_write(data->value, data->offset);
            break;
        default:
            return UACPI_STATUS_INVALID_ARGUMENT;
    }

    acpi_ec_burst_disable();

    return UACPI_STATUS_OK;
}

static uacpi_status acpi_ec_region_handle(uacpi_region_op op, uacpi_handle op_data)
{
    switch (op)
    {
        case UACPI_REGION_OP_ATTACH:
        case UACPI_REGION_OP_DETACH:
            return UACPI_STATUS_OK;
        default:
            return acpi_ec_rw(op, op_data);
    }
}

uacpi_interrupt_ret acpi_ec_event_handle(uacpi_handle ctx, uacpi_namespace_node*, uacpi_u16)
{
    UNUSED(ctx);
    return UACPI_INTERRUPT_NOT_HANDLED;
}

UNMAP_AFTER_INIT uacpi_iteration_decision acpi_ec_init(void*, uacpi_namespace_node* node, uint32_t)
{
    uacpi_resources* res;

    uacpi_status ret = uacpi_get_current_resources(node, &res);

    if (uacpi_unlikely_error(ret))
    {
        log_warning("uacpi_get_current_resources: %s", uacpi_status_to_string(ret));
        goto finish;
    }

    uacpi_resource* entry = res->entries;
    uacpi_resource* end = shift(entry, res->length);

    size_t idx = 0;

    for (; entry < end; entry = shift(entry, entry->length))
    {
        struct acpi_gas* reg = idx ? &acpi_ec.control : &acpi_ec.data;

        switch (entry->type)
        {
            case UACPI_RESOURCE_TYPE_IO:
                reg->address = entry->io.minimum;
                reg->register_bit_width = entry->io.length * 8;
                break;
            case UACPI_RESOURCE_TYPE_FIXED_IO:
                reg->address = entry->fixed_io.address;
                reg->register_bit_width = entry->fixed_io.length * 8;
                break;
        }

        reg->address_space_id = UACPI_ADDRESS_SPACE_SYSTEM_IO;

        if (++idx == 2)
        {
            break;
        }
    }

    if (unlikely(idx != 2))
    {
        log_warning("cannot read IO registers");
        goto finish;
    }

    acpi_ec.node = node;

    log_notice("ctrl: %#x; data: %#x", (uint32_t)acpi_ec.control.address, (uint32_t)acpi_ec.data.address);

    ret = uacpi_install_address_space_handler(
        node,
        UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER,
        acpi_ec_region_handle,
        NULL);

    if (uacpi_unlikely_error(ret))
    {
        log_warning("cannot install address space handler: %s", uacpi_status_to_string(ret));
        goto finish;
    }

    uint64_t value = 0;
    uacpi_eval_simple_integer(node, "_GLK", &value);

    ret = uacpi_eval_simple_integer(node, "_GPE", &value);
    if (uacpi_unlikely_error(ret))
    {
        log_warning("cannot evaluate _GPE: %s", uacpi_status_to_string(ret));
        goto finish;
    }

    ret = uacpi_install_gpe_handler(
        NULL,
        value,
        UACPI_GPE_TRIGGERING_EDGE,
        acpi_ec_event_handle,
        NULL);

    if (uacpi_unlikely_error(ret))
    {
        log_warning("cannot install _GPE handler: %s", uacpi_status_to_string(ret));
    }

finish:
    return UACPI_ITERATION_DECISION_BREAK;
}
