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

static uint16_t pci_config_read_u16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    outl(0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc), PCI_CONFIG_ADDRESS);
    return inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8) & 0xffff;
}

static uint32_t pci_config_read_u32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    outl(0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | offset, PCI_CONFIG_ADDRESS);
    return inl(PCI_CONFIG_DATA);
}

static void pci_config_write_u32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val)
{
    outl(0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | offset, PCI_CONFIG_ADDRESS);
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
    for (; size >= 4; size -= 4, buf++, offset += 4)
    {
        *buf = pci_config_read_u32(device->bus, device->slot, device->func, offset);
    }
    uint16_t* buf16 = ptr(buf);
    for (; size >= 2; size -= 2, buf16++, offset += 2)
    {
        *buf16 = pci_config_read_u16(device->bus, device->slot, device->func, offset);
    }

    if (size)
    {
        return -1;
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
        log_info("PCI BIOS entry: %04x:%08x", pci_bios_entry.seg, pci_bios_entry.addr);

        regs_t regs = {};

        bios32_call(&pci_bios_entry, PCI_BIOS_CHECK(regs));

        if (regs.ah != PCI_BIOS_SUCCESSFUL)
        {
            log_info("PCI BIOS call unsuccessful: %x", regs.ah);
            goto skip_pci_bios;
        }

        log_continue("; config mechanism: %x", regs.al & 0x3);
        log_continue("; special cycle: %x", (regs.al >> 4) & 0x3);
        log_continue("; last bus: %x", regs.cl);
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
    it = csnprintf(it, end, " [%02X%02X]: ", device->class, device->subclass);
    switch (device->vendor_id)
    {
        case PCI_AMD:
            it = csnprintf(it, end, "Advanced Micro Devices, Inc. [AMD/ATI] ");
            switch (device->device_id)
            {
                case 0x4e50:
                    it = csnprintf(it, end, "RV350/M10 / RV360/M11 [Mobility Radeon 9600 (PRO) / 9700]");
                    break;
                default:
                    it = csnprintf(it, end, "<unknown>");
                    break;
            }
            break;
        case PCI_TI:
            it = csnprintf(it, end, "Texas Instruments ");
            switch (device->device_id)
            {
                case 0xac46:
                    it = csnprintf(it, end, "PCI4520 PC card Cardbus Controller");
                    break;
                default:
                    it = csnprintf(it, end, "<unknown>");
                    break;
            }
            break;
        case PCI_VIRTIO:
            it = csnprintf(it, end, "Red Hat, Inc. ");
            switch (device->device_id)
            {
                case 0x1001:
                    it = csnprintf(it, end, "VirtIO block device");
                    break;
                case 0x1003:
                    it = csnprintf(it, end, "VirtIO console");
                    break;
                case 0x1050:
                    it = csnprintf(it, end, "Virtio 1.0 GPU");
                    break;
                default:
                    it = csnprintf(it, end, "<unknown>");
                    break;
            }
            break;
        case PCI_INTEL:
            it = csnprintf(it, end, "Intel ");
            switch (device->device_id)
            {
                case 0x0154:
                    it = csnprintf(it, end, "3rd Gen Core processor DRAM Controller");
                    break;
                case 0x0166:
                    it = csnprintf(it, end, "3rd Gen Core processor Graphics Controller");
                    break;
                case 0x100e:
                    it = csnprintf(it, end, "82540EM Gigabit Ethernet Controller");
                    break;
                case 0x101e:
                    it = csnprintf(it, end, "82540EP Gigabit Ethernet Controller (Mobile)");
                    break;
                case 0x1503:
                    it = csnprintf(it, end, "82579V Gigabit Network Connection");
                    break;
                case 0x1e03:
                    it = csnprintf(it, end, "7 Series Chipset Family 6-port SATA Controller [AHCI mode]");
                    break;
                case 0x1e10:
                    it = csnprintf(it, end, "7 Series/C216 Chipset Family PCI Express Root Port 1");
                    break;
                case 0x1e12:
                    it = csnprintf(it, end, "7 Series/C210 Series Chipset Family PCI Express Root Port 2");
                    break;
                case 0x1e14:
                    it = csnprintf(it, end, "7 Series/C210 Series Chipset Family PCI Express Root Port 3");
                    break;
                case 0x1e16:
                    it = csnprintf(it, end, "7 Series/C216 Chipset Family PCI Express Root Port 4");
                    break;
                case 0x1e20:
                    it = csnprintf(it, end, "7 Series/C216 Chipset Family High Definition Audio Controller");
                    break;
                case 0x1e26:
                    it = csnprintf(it, end, "7 Series/C216 Chipset Family USB Enhanced Host Controller #1");
                    break;
                case 0x1e2d:
                    it = csnprintf(it, end, "7 Series/C216 Chipset Family USB Enhanced Host Controller #2");
                    break;
                case 0x1e31:
                    it = csnprintf(it, end, "7 Series/C210 Series Chipset Family USB xHCI Host Controller");
                    break;
                case 0x1e3a:
                    it = csnprintf(it, end, "7 Series/C216 Chipset Family MEI Controller #1");
                    break;
                case 0x1e59:
                    it = csnprintf(it, end, "HM76 Express Chipset LPC Controller");
                    break;
                case 0x1237:
                    it = csnprintf(it, end, "440FX - 82441FX PMC [Natoma]");
                    break;
                case 0x2448:
                    it = csnprintf(it, end, "82801 Mobile PCI Bridge");
                    break;
                case 0x24c2:
                    it = csnprintf(it, end, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) USB UHCI Controller #1");
                    break;
                case 0x24c3:
                    it = csnprintf(it, end, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) SMBus Controller");
                    break;
                case 0x24c4:
                    it = csnprintf(it, end, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) USB UHCI Controller #2");
                    break;
                case 0x24c5:
                    it = csnprintf(it, end, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) AC'97 Audio Controller");
                    break;
                case 0x24c6:
                    it = csnprintf(it, end, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) AC'97 Modem Controller");
                    break;
                case 0x24c7:
                    it = csnprintf(it, end, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) USB UHCI Controller #3");
                    break;
                case 0x24ca:
                    it = csnprintf(it, end, "82801DBM (ICH4-M) IDE Controller");
                    break;
                case 0x24cc:
                    it = csnprintf(it, end, "82801DBM (ICH4-M) LPC Interface Bridge");
                    break;
                case 0x24cd:
                    it = csnprintf(it, end, "82801DB/DBM (ICH4/ICH4-M) USB2 EHCI Controller");
                    break;
                case 0x2922:
                    it = csnprintf(it, end, "82801IR/IO/IH (ICH9R/DO/DH) 6 port SATA Controller [AHCI mode]");
                    break;
                case 0x3340:
                    it = csnprintf(it, end, "82855PM Processor to I/O Controller");
                    break;
                case 0x3341:
                    it = csnprintf(it, end, "82855PM Processor to AGP Controller");
                    break;
                case 0x7000:
                    it = csnprintf(it, end, "82371SB PIIX3 ISA [Natoma/Triton II]");
                    break;
                case 0x7010:
                    it = csnprintf(it, end, "82371SB PIIX3 IDE [Natoma/Triton II]");
                    break;
                case 0x7113:
                    it = csnprintf(it, end, "82371AB/EB/MB PIIX4 ACPI");
                    break;
                case 0x7020:
                    it = csnprintf(it, end, "82371SB PIIX3 USB [Natoma/Triton II]");
                    break;
                default:
                    it = csnprintf(it, end, "<unknown>");
                    break;
            }
            break;
        case PCI_QEMU:
            it = csnprintf(it, end, "QEMU ");
            switch (device->device_id)
            {
                case 0x1111:
                    it = csnprintf(it, end, "Virtual Video Controller");
                    break;
                default:
                    it = csnprintf(it, end, "<unknown>");
                    break;
            }
            break;
        case PCI_QUALCOMM:
            it = csnprintf(it, end, "Qualcomm Atheros ");
            switch (device->device_id)
            {
                case 0x0013:
                    it = csnprintf(it, end, "AR5212/5213/2414 Wireless Network Adapter");
                    break;
                default:
                    it = csnprintf(it, end, "<unknown>");
                    break;
            }
            break;
        case PCI_VMWARE:
            it = csnprintf(it, end, "VMware ");
            switch (device->device_id)
            {
                case 0x0405:
                    it = csnprintf(it, end, "SVGA II Adapter");
                    break;
                default:
                    it = csnprintf(it, end, "<unknown>");
                    break;
            }
            break;
        default:
            it = csnprintf(it, end, "Device");
    }
    it = csnprintf(it, end, " [%04X:%04X]", device->vendor_id, device->device_id);

    return buf;
}

static inline char* pci_device_subsystem_description(pci_device_t* device, char* buf, size_t size)
{
    char* it = buf;
    const char* end = buf + size;

    switch (device->subsystem_vendor_id)
    {
        case 0x1014:
            it = csnprintf(it, end, "IBM");
            break;
        case 0x103c:
            it = csnprintf(it, end, "Hewlett-Packard Company");
            break;
        case 0x1af4:
            it = csnprintf(it, end, "QEMU Virtual Machine");
            break;
        default:
            it = csnprintf(it, end, "Unknown");
            break;
    }
    it = csnprintf(it, end, " [%04X:%04X]", device->subsystem_vendor_id, device->subsystem_id);
    return buf;
}

static inline char* pci_bar_description(bar_t* bar, char* buf, size_t size)
{
    const char* unit = "B";
    uint32_t bar_size = bar->size;

    if (bar->region == PCI_MEMORY)
    {
        human_size(bar_size, unit);
        snprintf(buf, size, "memory %x [size=%u %s]", bar->addr, bar_size, unit);
    }
    else
    {
        snprintf(buf, size, "io %x [size=%u]", bar->addr, bar_size);
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

    list_init(&device->list_entry);
    device->bus = bus;
    device->slot = slot;
    device->func = func;
    list_add_tail(&device->list_entry, &pci_devices);
}

void pci_device_print(pci_device_t* device)
{
    char description[256] = {0, };

    log_notice("%X:%X.%X %s",
        device->bus, device->slot, device->func,
        pci_device_description(device, description, sizeof(description)));

    description[0] = 0;
    log_notice("\tSubsystem: %s", pci_device_subsystem_description(device, description, sizeof(description)));
    log_notice("\tStatus: %04x", device->status);
    log_notice("\tCommand: %04x", device->command);
    log_notice("\tProg IF: %02x", device->prog_if);
    if (device->interrupt_line)
    {
        log_notice("\tInterrupt: pin %u IRQ %u", device->interrupt_pin, device->interrupt_line);
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
            log_notice("\tBAR%u: %s", i, pci_bar_description(bar, description, sizeof(description)));
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
