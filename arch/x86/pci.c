#include <arch/io.h>
#include <arch/pci.h>
#include <arch/bios32.h>
#include <kernel/init.h>
#include <kernel/kernel.h>

static void pci_device_add(uint32_t vendor, uint8_t bus, uint8_t slot, uint8_t func);

static LIST_DECLARE(pci_devices);

#define PCI_BIOS_SIGNATURE U32('$', 'P', 'C', 'I')

#define PCI_BIOS_SUCCESSFUL             0x00
#define PCI_BIOS_FUNC_NOT_SUPPORTED     0x81
#define PCI_BIOS_BAD_VENDOR_ID          0x83
#define PCI_BIOS_DEVICE_NOT_FOUND       0x86
#define PCI_BIOS_BAD_REGISTER_NUMBER    0x87
#define PCI_BIOS_SET_FAILED             0x88
#define PCI_BIOS_BUFFER_TOO_SMALL       0x89

#define PCI_BIOS_CHECK(regs) \
    ({ \
        regs.ah = 0xb1; \
        regs.al = 0x01; \
        &regs; \
    })

#ifdef __i386__
static bios32_entry_t pci_bios_entry;
#endif

static int pci_devices_list(void)
{
    pci_device_t* device;

    list_for_each_entry(device, &pci_devices, list_entry)
    {
        pci_device_print(device);
    }
    return 0;
}

static uint32_t pci_io_address(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    return 0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc);
}

static uint16_t pci_config_read_u16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    outl(pci_io_address(bus, slot, func, offset), PCI_CONFIG_ADDRESS);
    return inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8) & 0xffff;
}

static uint32_t pci_config_read_u32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    outl(pci_io_address(bus, slot, func, offset), PCI_CONFIG_ADDRESS);
    return inl(PCI_CONFIG_DATA);
}

static void pci_config_write_u32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val)
{
    outl(pci_io_address(bus, slot, func, offset), PCI_CONFIG_ADDRESS);
    outl(val, PCI_CONFIG_DATA);
}

int pci_device_initialize(pci_device_t* device)
{
    device->command |= (1 << 2) | (1 << 1) | 1;
    ASSERT(device->command & (1 << 2));
    ASSERT(device->command & (1 << 1));
    ASSERT(device->command & 1);

    return !(device->command & 1);
}

int pci_config_read(pci_device_t* device, uint8_t offset, void* buffer, size_t size)
{
    uint32_t* buf = buffer;

    if (unlikely(size % 4))
    {
        log_error("invalid read from PCI device");
        return -1;
    }

    for (; size >= 4; size -= 4, buf++, offset += 4)
    {
        *buf = pci_config_read_u32(device->bus, device->slot, device->func, offset);
    }

    return 0;
}

int pci_config_write(pci_device_t* device, uint8_t offset, const void* buffer, size_t size)
{
    const uint32_t* buf = buffer;

    if (unlikely(size % 4))
    {
        log_error("invalid read from PCI device");
        return -1;
    }

    for (; size >= 4; size -= 4, buf++, offset += 4)
    {
        pci_config_write_u32(device->bus, device->slot, device->func, offset, *buf);
    }

    return 0;
}

UNMAP_AFTER_INIT void pci_scan(void)
{
    uint32_t bus, slot, func;
    uint16_t vendor_id, header_type;
    uint32_t bus_size = 32;

    scoped_irq_lock();

#ifdef __i386__
    if (!bios32_find(PCI_BIOS_SIGNATURE, &pci_bios_entry))
    {
        log_info("PCI BIOS entry: %#06x:%#010x", pci_bios_entry.seg, pci_bios_entry.addr);

        regs_t regs = {};

        bios32_call(&pci_bios_entry, PCI_BIOS_CHECK(regs));

        if (regs.ah != PCI_BIOS_SUCCESSFUL)
        {
            log_info("PCI BIOS call unsuccessful: %#x", regs.ah);
            goto skip_pci_bios;
        }

        log_continue("; config mechanism: %#x", regs.al & 0x3);
        log_continue("; special cycle: %#x", (regs.al >> 4) & 0x3);
        log_continue("; last bus: %#x", regs.cl);
        bus_size = regs.cl + 1;
    }
#else
    goto skip_pci_bios;
#endif

skip_pci_bios:
    for (bus = 0; bus < bus_size; ++bus)
    {
        for (slot = 0; slot < 32; ++slot)
        {
            vendor_id = pci_config_read_u16(bus, slot, 0, VENDOR_DEVICE_ID);
            if (vendor_id == 0xffff)
            {
                continue;
            }

            header_type = pci_config_read_u16(bus, slot, 0, HEADER_TYPE) & 0xff;
            pci_device_add(vendor_id, bus, slot, 0);

            if (header_type & 0x80)
            {
                for (func = 1; func < 8; ++func)
                {
                    vendor_id = pci_config_read_u16(bus, slot, func, VENDOR_DEVICE_ID);
                    if (vendor_id != 0xffff)
                    {
                        pci_device_add(vendor_id, bus, slot, func);
                    }
                }
            }
        }
    }

    param_call_if_set(KERNEL_PARAM("pciprint"), &pci_devices_list);
}

#define DEVICE_ID(id, name) \
    case id: it = csnprintf(it, end, name); break

#define UNKNOWN_DEVICE_ID() \
    default: it = csnprintf(it, end, "<unknown>"); break

#define VENDOR_ID(id, name) \
    case id: \
        it = csnprintf(it, end, name " "); \
        switch (device->device_id)

#define UNKNOWN_VENDOR_ID() \
    default: it = csnprintf(it, end, "Device")

static inline char* pci_device_description(pci_device_t* device, char* buf, size_t size)
{
    char* it = buf;
    const char* end = buf + size;

    switch (device->class)
    {
        case PCI_UNCLASSIFIED:
            it = csnprintf(it, end, "Unclassified");
            break;
        case PCI_STORAGE:
            it = csnprintf(it, end, storage_subclass_string(device->subclass));
            break;
        case PCI_NETWORK:
            it = csnprintf(it, end, "Network controller");
            break;
        case PCI_DISPLAY:
            it = csnprintf(it, end, display_subclass_string(device->subclass));
            break;
        case PCI_MULTIMEDIA:
            it = csnprintf(it, end, multimedia_subclass_string(device->subclass));
            break;
        case PCI_BRIDGE:
            it = csnprintf(it, end, bridge_subclass_string(device->subclass));
            break;
        case PCI_COMCONTROLLER:
            it = csnprintf(it, end, "Communication controller");
            break;
        case PCI_SERIAL_BUS:
            it = csnprintf(it, end, serial_bus_subclass_string(device->subclass));
            break;
    }
    it = csnprintf(it, end, " [%02x%02x]: ", device->class, device->subclass);
    switch (device->vendor_id)
    {
        VENDOR_ID(PCI_AMD, "Advanced Micro Devices, Inc. [AMD/ATI]")
        {
            DEVICE_ID(0x4e50, "RV350/M10 / RV360/M11 [Mobility Radeon 9600 (PRO) / 9700]");
            DEVICE_ID(0x5046, "Rage 4 [Rage 128 PRO AGP 4X]");
            UNKNOWN_DEVICE_ID();
        }
        break;

        VENDOR_ID(PCI_TI, "Texas Instruments")
        {
            DEVICE_ID(0xac46, "PCI4520 PC card Cardbus Controller");
            UNKNOWN_DEVICE_ID();
        }
        break;

        VENDOR_ID(PCI_VIRTIO, "Red Hat, Inc.")
        {
            DEVICE_ID(0x1001, "VirtIO block device");
            DEVICE_ID(0x1003, "VirtIO console");
            DEVICE_ID(0x1050, "Virtio 1.0 GPU");
            UNKNOWN_DEVICE_ID();
        }
        break;

        VENDOR_ID(PCI_INTEL, "Intel")
        {
            DEVICE_ID(0x0154, "3rd Gen Core processor DRAM Controller");
            DEVICE_ID(0x0166, "3rd Gen Core processor Graphics Controller");
            DEVICE_ID(0x100e, "82540EM Gigabit Ethernet Controller");
            DEVICE_ID(0x101e, "82540EP Gigabit Ethernet Controller (Mobile)");
            DEVICE_ID(0x1503, "82579V Gigabit Network Connection");
            DEVICE_ID(0x1e03, "7 Series Chipset Family 6-port SATA Controller [AHCI mode]");
            DEVICE_ID(0x1e10, "7 Series/C216 Chipset Family PCI Express Root Port 1");
            DEVICE_ID(0x1e12, "7 Series/C210 Series Chipset Family PCI Express Root Port 2");
            DEVICE_ID(0x1e14, "7 Series/C210 Series Chipset Family PCI Express Root Port 3");
            DEVICE_ID(0x1e16, "7 Series/C216 Chipset Family PCI Express Root Port 4");
            DEVICE_ID(0x1e20, "7 Series/C216 Chipset Family High Definition Audio Controller");
            DEVICE_ID(0x1e26, "7 Series/C216 Chipset Family USB Enhanced Host Controller #1");
            DEVICE_ID(0x1e2d, "7 Series/C216 Chipset Family USB Enhanced Host Controller #2");
            DEVICE_ID(0x1e31, "7 Series/C210 Series Chipset Family USB xHCI Host Controller");
            DEVICE_ID(0x1e3a, "7 Series/C216 Chipset Family MEI Controller #1");
            DEVICE_ID(0x1e59, "HM76 Express Chipset LPC Controller");
            DEVICE_ID(0x1237, "440FX - 82441FX PMC [Natoma]");
            DEVICE_ID(0x2448, "82801 Mobile PCI Bridge");
            DEVICE_ID(0x24c2, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) USB UHCI Controller #1");
            DEVICE_ID(0x24c3, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) SMBus Controller");
            DEVICE_ID(0x24c4, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) USB UHCI Controller #2");
            DEVICE_ID(0x24c5, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) AC'97 Audio Controller");
            DEVICE_ID(0x24c6, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) AC'97 Modem Controller");
            DEVICE_ID(0x24c7, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) USB UHCI Controller #3");
            DEVICE_ID(0x24ca, "82801DBM (ICH4-M) IDE Controller");
            DEVICE_ID(0x24cc, "82801DBM (ICH4-M) LPC Interface Bridge");
            DEVICE_ID(0x24cd, "82801DB/DBM (ICH4/ICH4-M) USB2 EHCI Controller");
            DEVICE_ID(0x2922, "82801IR/IO/IH (ICH9R/DO/DH) 6 port SATA Controller [AHCI mode]");
            DEVICE_ID(0x3340, "82855PM Processor to I/O Controller");
            DEVICE_ID(0x3341, "82855PM Processor to AGP Controller");
            DEVICE_ID(0x7000, "82371SB PIIX3 ISA [Natoma/Triton II]");
            DEVICE_ID(0x7010, "82371SB PIIX3 IDE [Natoma/Triton II]");
            DEVICE_ID(0x7113, "82371AB/EB/MB PIIX4 ACPI");
            DEVICE_ID(0x7020, "82371SB PIIX3 USB [Natoma/Triton II]");
            UNKNOWN_DEVICE_ID();
        }
        break;

        VENDOR_ID(PCI_QEMU, "QEMU")
        {
            DEVICE_ID(0x1111, "Virtual Video Controller");
            UNKNOWN_DEVICE_ID();
        }
        break;

        VENDOR_ID(PCI_QUALCOMM, "Qualcomm Atheros")
        {
            DEVICE_ID(0x0013, "AR5212/5213/2414 Wireless Network Adapter");
            UNKNOWN_DEVICE_ID();
        }
        break;

        VENDOR_ID(PCI_VMWARE, "VMware")
        {
            DEVICE_ID(0x0405, "SVGA II Adapter");
            UNKNOWN_DEVICE_ID();
        }
        break;

        VENDOR_ID(PCI_CIRRUS, "Cirrus Logic")
        {
            DEVICE_ID(0x00b8, "GD 5446");
            UNKNOWN_DEVICE_ID();
        }
        break;

        UNKNOWN_VENDOR_ID();
    }
    it = csnprintf(it, end, " [%04x:%04x]", device->vendor_id, device->device_id);

    return buf;
}

#define SUBSYSTEM_VENDOR_ID(id, name) \
    case id: it = csnprintf(it, end, name); break

#define UNKNOWN_SUBSYSTEM_VENDOR_ID() \
    default: it = csnprintf(it, end, "Unknown"); break

static inline char* pci_device_subsystem_description(pci_device_t* device, char* buf, size_t size)
{
    char* it = buf;
    const char* end = buf + size;

    switch (device->subsystem_vendor_id)
    {
        SUBSYSTEM_VENDOR_ID(0x1014, "IBM");
        SUBSYSTEM_VENDOR_ID(0x103c, "Hewlett-Packard Company");
        SUBSYSTEM_VENDOR_ID(0x1af4, "QEMU Virtual Machine");
        UNKNOWN_SUBSYSTEM_VENDOR_ID();
    }
    it = csnprintf(it, end, " [%04x:%04x]", device->subsystem_vendor_id, device->subsystem_id);
    return buf;
}

static inline char* pci_bar_description(bar_t* bar, char* buf, size_t size)
{
    const char* unit = "B";
    uint32_t bar_size = bar->size;

    if (bar->region == PCI_MEMORY)
    {
        human_size(bar_size, unit);
        snprintf(buf, size, "memory %#x [size=%u %s]", bar->addr, bar_size, unit);
    }
    else
    {
        snprintf(buf, size, "io %#x [size=%u]", bar->addr, bar_size);
    }

    return buf;
}

static void pci_device_add(uint32_t, uint8_t bus, uint8_t slot, uint8_t func)
{
    pci_device_t* device = alloc(pci_device_t);

    if (unlikely(!device))
    {
        log_error("cannot allocate pci_device");
        return;
    }

    memset(device, 0, sizeof(pci_device_t));

    uint32_t* buf = ptr(device);

    for (uint8_t addr = 0; addr < BAR0; addr += 4)
    {
        *buf++ = pci_config_read_u32(bus, slot, func, addr);
    }

    if ((device->header_type & 0x7f) == 0)
    {
        for (uint32_t addr = BAR0; addr < BAR_END; addr += 4)
        {
            bar_t* bar = ptr(buf);
            uint32_t raw_bar = pci_config_read_u32(bus, slot, func, addr);
            if (raw_bar)
            {
                bar->region = raw_bar & 0x1;
                if (bar->region == PCI_MEMORY)
                {
                    bar->space = (raw_bar >> 1) & 0x3;
                    bar->addr = raw_bar & ~0xf;
                }
                else
                {
                    bar->space = 0;
                    bar->addr = raw_bar & ~0x1;
                }
                pci_config_write_u32(bus, slot, func, addr, ~(uint32_t)0);
                bar->size = ~(pci_config_read_u32(bus, slot, func, addr) & ~0xf) + 1;
                pci_config_write_u32(bus, slot, func, addr, raw_bar);
            }
            buf = ptr(addr(buf) + sizeof(bar_t));
        }
        for (uint32_t addr = BAR_END; addr < HEADER0_END; addr += 4)
        {
            *buf++ = pci_config_read_u32(bus, slot, func, addr);
        }
    }
    else
    {
        for (uint32_t addr = BAR0; addr < HEADER0_END; addr += 4)
        {
            *buf++ = pci_config_read_u32(bus, slot, func, addr);
        }
    }

    mb();

    list_init(&device->list_entry);
    device->bus = bus;
    device->slot = slot;
    device->func = func;
    device->capabilities &= ~3;
    list_add_tail(&device->list_entry, &pci_devices);
}

void pci_device_print(pci_device_t* device)
{
    char description[256] = {0, };

    log_notice("%x:%x.%x %s",
        device->bus, device->slot, device->func,
        pci_device_description(device, description, sizeof(description)));

    description[0] = 0;
    log_notice("  Subsystem: %s", pci_device_subsystem_description(device, description, sizeof(description)));
    log_notice("  Status: %#06x", device->status);
    log_notice("  Command: %#06x", device->command);
    log_notice("  Prog IF: %#04x", device->prog_if);
    if (device->interrupt_line)
    {
        log_notice("  Interrupt: pin %u IRQ %u", device->interrupt_pin, device->interrupt_line);
    }

    if ((device->header_type & 0x7f) == 0)
    {
        for (int i = 0; i < 6; ++i)
        {
            bar_t* bar = device->bar + i;
            if (!bar->addr)
            {
                continue;
            }
            log_notice("  BAR%u: %s", i, pci_bar_description(bar, description, sizeof(description)));
        }
    }
}

pci_device_t* pci_device_get(uint8_t class, uint8_t subclass)
{
    pci_device_t* device;
    list_for_each_entry(device, &pci_devices, list_entry)
    {
        if (device->class == class && device->subclass == subclass)
        {
            return device;
        }
    }
    return NULL;
}
