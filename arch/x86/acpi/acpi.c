#define log_fmt(fmt) "acpi: " fmt
#include <arch/io.h>
#include <arch/pci.h>
#include <arch/acpi.h>
#include <arch/bios.h>
#include <arch/bios32.h>
#include <arch/reboot.h>

#include <kernel/kernel.h>
#include <kernel/compiler.h>
#include <kernel/page_mmio.h>

#include <uacpi/acpi.h>
#include <uacpi/event.h>
#include <uacpi/sleep.h>
#include <uacpi/uacpi.h>
#include <uacpi/status.h>
#include <uacpi/tables.h>
#include <uacpi/resources.h>
#include <uacpi/utilities.h>

#include "ec.h"

#define UACPI_DISABLED 0
#define UACPI_NS_PRINT 0
#define DEBUG_RSDT     0
#define RSDP_SIGNATURE U32('R', 'S', 'D', ' ')

READONLY static uint16_t reset_port;
READONLY static uint8_t reset_value;
READONLY static bool acpi_available;
READONLY bool acpi_enabled;

static void acpi_reboot(void)
{
    log_notice("performing reboot");
    outb(reset_value, reset_port);
    for (;; halt());
}

static void acpi_shutdown(void)
{
    log_notice("performing shutdown");

    uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(ret))
    {
        log_error("failed to prepare for sleep: %s", uacpi_status_to_string(ret));
        return;
    }

    ret = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(ret))
    {
        log_error("failed to enter sleep: %s", uacpi_status_to_string(ret));
        return;
    }

    for (;; halt());
}

static const char* uacpi_trigger_string(int type)
{
    switch (type)
    {
        case UACPI_TRIGGERING_EDGE:  return "edge";
        case UACPI_TRIGGERING_LEVEL: return "level";
        default:                     return "unknown";
    }
}

static const char* uacpi_polarity_string(int type)
{
    switch (type)
    {
        case UACPI_POLARITY_ACTIVE_HIGH: return "high";
        case UACPI_POLARITY_ACTIVE_LOW:  return "low";
        case UACPI_POLARITY_ACTIVE_BOTH: return "both";
        default:                         return "unknown";
    }
}

static uacpi_iteration_decision acpi_dev_print(void*, uacpi_namespace_node* node, uacpi_u32)
{
    uacpi_namespace_node_info* info;

    uacpi_status ret = uacpi_get_namespace_node_info(node, &info);

    if (uacpi_unlikely_error(ret))
    {
        const char *path = uacpi_namespace_node_generate_absolute_path(node);
        log_error("unable to retrieve node %s information: %s",
                  path, uacpi_status_to_string(ret));
        uacpi_free_absolute_path(path);
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    const char* path = uacpi_namespace_node_generate_absolute_path(node);

    log_notice("%s", path);

    if (info->flags & UACPI_NS_NODE_INFO_HAS_HID)
    {
        log_continue("; HID: %s", info->hid.value);
        if (!strcmp("PNP0A03", info->hid.value))
        {
            uacpi_pci_routing_table* table = NULL;
            ret = uacpi_get_pci_routing_table(node, &table);

            if (uacpi_unlikely_error(ret))
            {
                log_error("unable to retrieve node %s information: %s",
                      path, uacpi_status_to_string(ret));
            }

            for (size_t i = 0; i < table->num_entries; ++i)
            {
                path = uacpi_namespace_node_generate_absolute_path(table->entries[i].source);
                log_notice("  PCI IRQ: %p id %u; pin %u %s",
                    table->entries[i].address,
                    table->entries[i].index,
                    table->entries[i].pin,
                    path);
            }
        }
    }

    if (info->flags & UACPI_NS_NODE_INFO_HAS_ADR)
    {
        log_continue("; ADR: %#llx", info->adr);
    }

    if (info->flags & UACPI_NS_NODE_INFO_HAS_CID)
    {
        log_continue("; CID: [", info->cid.num_ids);
        for (size_t i = 0; i < info->cid.num_ids; ++i)
        {
            log_continue("%s%s", i == 0 ? "" : ", ", info->cid.ids[i].value);
        }
        log_continue("]");
    }

    if (info->flags & UACPI_NS_NODE_INFO_HAS_CLS)
    {
        log_continue("; CLS");
    }

    if (info->flags & UACPI_NS_NODE_INFO_HAS_UID)
    {
        log_continue("; UID: %s", info->uid.value);
    }

    if (info->flags & UACPI_NS_NODE_INFO_HAS_SXD)
    {
        log_continue("; SXD");
    }

    if (info->flags & UACPI_NS_NODE_INFO_HAS_SXW)
    {
        log_continue("; SXW");
    }

    uacpi_resources* res = NULL;
    uacpi_status st = uacpi_get_current_resources(node, &res);

    if (st == UACPI_STATUS_OK)
    {
        uacpi_resource* entry = res->entries;
        uacpi_resource* end = shift(entry, res->length);

        log_notice("  Resources:");
        for (; entry < end; entry = shift(entry, entry->length))
        {
            if (entry->type == UACPI_RESOURCE_TYPE_END_TAG)
            {
                break;
            }
            log_notice("    ");
            switch (entry->type)
            {
                case UACPI_RESOURCE_TYPE_IRQ:
                    log_continue("IRQ: trigger: %s; pol: %s; [",
                        uacpi_trigger_string(entry->irq.triggering),
                        uacpi_polarity_string(entry->irq.polarity));
                    for (size_t i = 0; i < entry->irq.num_irqs; ++i)
                    {
                        log_continue("%s%u",
                            i == 0 ? "" : ", ",
                            entry->irq.irqs[i]);
                    }
                    log_continue("]");
                    break;
                case UACPI_RESOURCE_TYPE_EXTENDED_IRQ:
                    log_continue("EXTENDED_IRQ: trigger: %s; pol: %s; [",
                        uacpi_trigger_string(entry->extended_irq.triggering),
                        uacpi_polarity_string(entry->extended_irq.polarity));
                    for (size_t i = 0; i < entry->extended_irq.num_irqs; ++i)
                    {
                        log_continue("%s%u",
                            i == 0 ? "" : ", ",
                            entry->extended_irq.irqs[i]);
                    }
                    log_continue("]");
                    break;
                case UACPI_RESOURCE_TYPE_DMA:
                    log_continue("DMA:");
                    break;
                case UACPI_RESOURCE_TYPE_FIXED_DMA:
                    log_continue("FIXED_DMA:");
                    break;
                case UACPI_RESOURCE_TYPE_IO:
                    log_continue("IO: [%#06x - %#06x] len: %#x, align: %u, decode: %u",
                        entry->io.minimum,
                        entry->io.maximum,
                        entry->length,
                        entry->io.alignment,
                        entry->io.decode_type);
                    break;
                case UACPI_RESOURCE_TYPE_FIXED_IO:
                    log_continue("FIXED_IO: [%#06x - %#06x]",
                        entry->fixed_io.address,
                        entry->fixed_io.address + entry->fixed_io.length);
                    break;
                case UACPI_RESOURCE_TYPE_ADDRESS16:
                    log_continue("ADDRESS16: [%#06x - %#06x] len: %#x",
                        entry->address16.minimum,
                        entry->address16.maximum,
                        entry->address16.address_length);
                    break;
                case UACPI_RESOURCE_TYPE_ADDRESS32:
                    log_continue("ADDRESS32: [%#06x - %#06x] len: %#x",
                        entry->address32.minimum,
                        entry->address32.maximum,
                        entry->address32.address_length);
                    break;
                case UACPI_RESOURCE_TYPE_ADDRESS64:
                    log_continue("ADDRESS64: [%#06llx - %#06llx] len: %#llx",
                        entry->address64.minimum,
                        entry->address64.maximum,
                        entry->address64.address_length);
                    break;
                case UACPI_RESOURCE_TYPE_FIXED_MEMORY32:
                    log_continue("FIXED_MEMORY32: %#06x; len: %#x",
                        entry->fixed_memory32.address,
                        entry->fixed_memory32.length);
                    break;
            }
        }

        uacpi_free_resources(res);
    }

    uacpi_free_absolute_path(path);
    uacpi_free_namespace_node_info(info);

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static uacpi_interrupt_ret handle_power_button(uacpi_handle)
{
    log_notice("%s", __func__);
    return UACPI_INTERRUPT_HANDLED;
}

static int power_button_init(void)
{
    uacpi_status ret = uacpi_install_fixed_event_handler(
        UACPI_FIXED_EVENT_POWER_BUTTON,
        handle_power_button, UACPI_NULL);

    if (uacpi_unlikely_error(ret))
    {
        log_error("failed to install power button event handler: %s", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    return 0;
}

void* acpi_find(char* signature)
{
    uacpi_table table;
    uacpi_status ret = uacpi_table_find_by_signature(signature, &table);

    if (uacpi_unlikely_error(ret))
    {
        return NULL;
    }

    return table.ptr;
}

UNMAP_AFTER_INIT void acpi_finalize(void)
{
    if (UACPI_DISABLED || !acpi_available)
    {
        return;
    }

    uacpi_status ret = uacpi_namespace_load();
    if (uacpi_unlikely_error(ret))
    {
        log_error("uacpi_namespace_load: %s", uacpi_status_to_string(ret));
        return;
    }

    uacpi_find_devices("PNP0C09", &acpi_ec_init, NULL);

    ret = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(ret))
    {
        log_error("uacpi_namespace_initialize: %s", uacpi_status_to_string(ret));
        return;
    }

    ret = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(ret))
    {
        log_error("uacpi_finalize_gpe_initialization: %s", uacpi_status_to_string(ret));
        return;
    }

    if (UACPI_NS_PRINT)
    {
        uacpi_namespace_for_each_child(
            uacpi_namespace_root(),
            &acpi_dev_print,
            UACPI_NULL,
            UACPI_OBJECT_DEVICE_BIT,
            UACPI_MAX_DEPTH_ANY,
            UACPI_NULL);
    }

    power_button_init();

    struct acpi_fadt* fadt = NULL;

    ret = uacpi_table_fadt(&fadt);
    if (uacpi_unlikely_error(ret))
    {
        log_warning("uacpi_table_fadt: %s", uacpi_status_to_string(ret));
    }
    else if (fadt->hdr.revision >= 2)
    {
        log_notice("reset req: space: %#x, access_size: %#x, addr: %#llx, value: %#x",
            fadt->reset_reg.address_space_id, fadt->reset_reg.access_size,
            fadt->reset_reg.address, fadt->reset_value);

        if (fadt->reset_reg.address_space_id == ACPI_AS_ID_SYS_IO)
        {
            reset_port = fadt->reset_reg.address;
            reset_value = fadt->reset_value;
            reboot_fn = &acpi_reboot;
        }
    }

    shutdown_fn = &acpi_shutdown;
    acpi_enabled = true;
}

UNMAP_AFTER_INIT void acpi_initialize(void)
{
    if (UACPI_DISABLED)
    {
        log_notice("uACPI disabled");
        return;
    }

    uacpi_status ret = uacpi_initialize(0);

    if (unlikely(ret == UACPI_STATUS_NOT_FOUND))
    {
        return;
    }

    if (uacpi_unlikely_error(ret))
    {
        log_error("uacpi_initialize: %s", uacpi_status_to_string(ret));
        return;
    }

    acpi_available = true;
}
