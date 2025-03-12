#define log_fmt(fmt) "mp: " fmt
#include <arch/mp.h>
#include <arch/bios.h>
#include <kernel/kernel.h>
#include <kernel/sections.h>
#include <kernel/byteorder.h>

#define MP_SIGNATURE  U32('_', 'M', 'P', '_')
#define MP_TABLE_SIGNATURE  U32('P', 'C', 'M', 'P')

struct bus
{
    char    name[8];
    uint8_t type;
};

typedef struct bus bus_t;

enum
{
    BUS_ISA,
    BUS_PCI,
};

static void mp_string_copy(char* dest, const char* src, size_t n)
{
    for (size_t i = 0; i < n; ++i, ++src, ++dest)
    {
        if ((*dest = *src) == ' ')
        {
            break;
        }
    }
    *dest = 0;
}

static const char* mp_cpu_flags_string(char* buffer, size_t size, uint8_t flags)
{
    char* it = buffer;
    const char* end = buffer + size;
    if (!(flags & 1))
    {
        it = csnprintf(it, end, "unusable");
    }
    if (flags & 2)
    {
        it = csnprintf(it, end, "BSP");
    }
    return buffer;
}

static const char* mp_irq_type_string(uint8_t type)
{
    switch (type)
    {
        case MP_IRQ_TYPE_INT:    return "INT";
        case MP_IRQ_TYPE_NMI:    return "NMI";
        case MP_IRQ_TYPE_SMI:    return "SMI";
        case MP_IRQ_TYPE_EXTINT: return "ExtINT";
        default:                 return "unknown";
    }
}

static const char* mp_irq_polarity_string(uint8_t flags)
{
    switch (flags & 3)
    {
        case 0:  return "bus-spec";
        case 1:  return "active high";
        case 3:  return "active low";
        default: return "reserved";
    }
}

static const char* mp_irq_trigger_string(uint8_t flags)
{
    switch ((flags >> 2) & 3)
    {
        case 0:  return "bus-spec";
        case 1:  return "edge";
        case 3:  return "level";
        default: return "reserved";
    }
}

static size_t mp_cpu_handle(mp_cpu_t* cpu)
{
    char buffer[64];

    log_info("  CPU: lapic: %u, signature: %#x, %s",
        cpu->lapic_id,
        cpu->signature,
        mp_cpu_flags_string(buffer, sizeof(buffer), cpu->flags));

    return sizeof(*cpu);
}

static size_t mp_bus_handle(mp_bus_t* bus, bus_t* busses)
{
    log_info("  BUS: id: %#x, type: %.6s",
        bus->id,
        bus->type_string);

    mp_string_copy(busses[bus->id].name, bus->type_string, sizeof(busses->name));

    if (!strcmp(busses[bus->id].name, "PCI"))
    {
        busses[bus->id].type = BUS_PCI;
    }
    else if (!strcmp(busses[bus->id].name, "ISA"))
    {
        busses[bus->id].type = BUS_ISA;
    }

    return sizeof(*bus);
}

static size_t mp_ioapic_handle(mp_ioapic_t* ioapic)
{
    log_info("  IOAPIC: id: %u, ver: %#x, ptr: %p",
        ioapic->id,
        ioapic->ver,
        ioapic->ptr);

    return sizeof(*ioapic);
}

static size_t mp_ioapic_irq_handle(mp_ioapic_irq_t* irq, const bus_t* busses)
{
    log_info("  IOAPIC IRQ: %s, polarity: %s, trigger: %s, src: %s",
        mp_irq_type_string(irq->type),
        mp_irq_polarity_string(irq->flags),
        mp_irq_trigger_string(irq->flags),
        busses[irq->src_bus_id].name);

    if (busses[irq->src_bus_id].type == BUS_PCI)
    {
        log_continue(" INT%c# device %u",
            (irq->src_bus_irq & 3) + 'A',
            (irq->src_bus_irq >> 2) & 0x1f);
    }
    else
    {
        log_continue(" IRQ%u", irq->src_bus_irq);
    }

    log_continue(", dest: IOAPIC#%u INT %u", irq->dest_ioapic_id, irq->dest_ioapic_pin);

    return sizeof(*irq);
}

static size_t mp_lapic_irq_handle(mp_lapic_irq_t* irq, const bus_t* busses)
{
    log_info("  LAPIC IRQ: type: %s, polarity: %s, trigger: %s, bus: %s",
        mp_irq_type_string(irq->type),
        mp_irq_polarity_string(irq->flags),
        mp_irq_trigger_string(irq->flags),
        busses[irq->src_bus_id].name);

    log_continue(", IRQ%u, LAPIC: %#x, LINT%u",
        irq->src_bus_irq,
        irq->dest_lapic_id,
        irq->dest_lapic_pin);

    return sizeof(*irq);
}

static int mp_checksum_verify(void* data, size_t len)
{
    uint8_t checksum = 0;
    uint8_t* byte = data;

    for (size_t i = 0; i < len; ++i, ++byte)
    {
        checksum += *byte;
    }

    return checksum;
}

UNMAP_AFTER_INIT void mp_read(void)
{
    mp_t* mp = bios_find(MP_SIGNATURE);

    if (!mp)
    {
        log_notice("no MP tables");
        return;
    }

    uint8_t checksum;

    if (unlikely(checksum = mp_checksum_verify(mp, 16)))
    {
        log_notice("bad MP checksum: %#x", checksum);
        return;
    }

    mp_table_t* table = ptr(mp->mp_table);
    void* ptr = shift_as(void*, table, sizeof(mp_table_t));

    bus_t busses[16] = {};

    char oem_id[9];
    char product_id[13];

    mp_string_copy(oem_id, table->oem_id, sizeof(table->oem_id));
    mp_string_copy(product_id, table->product_id, sizeof(table->product_id));

    log_notice("rev: 1.%u, mp table: %p: %s %s (entries count: %u):",
        mp->spec_rev,
        mp->mp_table,
        oem_id,
        product_id,
        table->entry_count);

    for (size_t i = 0; i < table->entry_count; ++i)
    {
        switch (*(uint8_t*)ptr)
        {
            case MP_CPU:
                ptr += mp_cpu_handle(ptr(ptr));
                break;
            case MP_BUS:
                ptr += mp_bus_handle(ptr(ptr), busses);
                break;
            case MP_IOAPIC:
                ptr += mp_ioapic_handle(ptr(ptr));
                break;
            case MP_IOAPIC_IRQ:
                ptr += mp_ioapic_irq_handle(ptr(ptr), busses);
                break;
            case MP_LAPIC_IRQ:
                ptr += mp_lapic_irq_handle(ptr(ptr), busses);
                break;
            default:
                log_info("  unknown (%#x)", *(uint8_t*)ptr);
                ptr += 8;
                break;
        }
    }
}
