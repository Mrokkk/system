#include <arch/io.h>
#include <arch/pci.h>
#include <arch/bios32.h>
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

bios32_entry_t pci_bios_entry;

UNMAP_AFTER_INIT void pci_scan(void)
{
    uint32_t bus, slot, func;
    uint16_t vendor_id, header_type;
    uint32_t bus_size = 32;

    scoped_irq_lock();

    if (!bios32_find(PCI_BIOS_SIGNATURE, &pci_bios_entry))
    {
        log_info("PCI BIOS entry: %04x:%08x", pci_bios_entry.seg, pci_bios_entry.addr);

        regs_t regs;

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
#if 0
    pci_devices_list();
#endif
}

uint16_t pci_config_read_u16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    outl(0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc), PCI_CONFIG_ADDRESS);
    return inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8) & 0xffff;
}

uint32_t pci_config_read_u32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    outl(0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | offset, PCI_CONFIG_ADDRESS);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write_u32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val)
{
    outl(0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | offset, PCI_CONFIG_ADDRESS);
    outl(val, PCI_CONFIG_DATA);
}

static inline char* pci_device_description(char* buf, pci_device_t* device)
{
    int i = 0;
    switch (device->class)
    {
        case PCI_UNCLASSIFIED:
            i = sprintf(buf, "Unclassified");
            break;
        case PCI_STORAGE:
            i = sprintf(buf, storage_subclass_string(device->subclass));
            break;
        case PCI_NETWORK:
            i = sprintf(buf, "Network controller");
            break;
        case PCI_DISPLAY:
            i = sprintf(buf, display_subclass_string(device->subclass));
            break;
        case PCI_MULTIMEDIA:
            i = sprintf(buf, multimedia_subclass_string(device->subclass));
            break;
        case PCI_BRIDGE:
            i = sprintf(buf, bridge_subclass_string(device->subclass));
            break;
        case PCI_COMCONTROLLER:
            i = sprintf(buf, "Communication controller");
            break;
        case PCI_SERIAL_BUS:
            i = sprintf(buf, serial_bus_subclass_string(device->subclass));
            break;
    }
    i += sprintf(buf + i, " [%02X%02X]: ", device->class, device->subclass);
    switch (device->vendor_id)
    {
        case PCI_AMD:
            i += sprintf(buf + i, "Advanced Micro Devices, Inc. [AMD/ATI] ");
            switch (device->device_id)
            {
                case 0x4e50:
                    i += sprintf(buf + i, "RV350/M10 / RV360/M11 [Mobility Radeon 9600 (PRO) / 9700]");
                    break;
                default:
                    i += sprintf(buf + i, "<unknown>");
                    break;
            }
            break;
        case PCI_TI:
            i += sprintf(buf + i, "Texas Instruments ");
            switch (device->device_id)
            {
                case 0xac46:
                    i += sprintf(buf + i, "PCI4520 PC card Cardbus Controller");
                    break;
                default:
                    i += sprintf(buf + i, "<unknown>");
                    break;
            }
            break;
        case PCI_VIRTIO:
            i += sprintf(buf + i, "Red Hat, Inc. ");
            switch (device->device_id)
            {
                case 0x1001:
                    i += sprintf(buf + i, "VirtIO block device");
                    break;
                case 0x1003:
                    i += sprintf(buf + i, "VirtIO console");
                    break;
                case 0x1050:
                    i += sprintf(buf + i, "Virtio 1.0 GPU");
                    break;
                default:
                    i += sprintf(buf + i, "<unknown>");
                    break;
            }
            break;
        case PCI_INTEL:
            i += sprintf(buf + i, "Intel ");
            switch (device->device_id)
            {
                case 0x0154:
                    i += sprintf(buf + i, "3rd Gen Core processor DRAM Controller");
                    break;
                case 0x0166:
                    i += sprintf(buf + i, "3rd Gen Core processor Graphics Controller");
                    break;
                case 0x100e:
                    i += sprintf(buf + i, "82540EM Gigabit Ethernet Controller");
                    break;
                case 0x101e:
                    i += sprintf(buf + i, "82540EP Gigabit Ethernet Controller (Mobile)");
                    break;
                case 0x1503:
                    i += sprintf(buf + i, "82579V Gigabit Network Connection");
                    break;
                case 0x1e03:
                    i += sprintf(buf + i, "7 Series Chipset Family 6-port SATA Controller [AHCI mode]");
                    break;
                case 0x1e10:
                    i += sprintf(buf + i, "7 Series/C216 Chipset Family PCI Express Root Port 1");
                    break;
                case 0x1e12:
                    i += sprintf(buf + i, "7 Series/C210 Series Chipset Family PCI Express Root Port 2");
                    break;
                case 0x1e14:
                    i += sprintf(buf + i, "7 Series/C210 Series Chipset Family PCI Express Root Port 3");
                    break;
                case 0x1e16:
                    i += sprintf(buf + i, "7 Series/C216 Chipset Family PCI Express Root Port 4");
                    break;
                case 0x1e20:
                    i += sprintf(buf + i, "7 Series/C216 Chipset Family High Definition Audio Controller");
                    break;
                case 0x1e26:
                    i += sprintf(buf + i, "7 Series/C216 Chipset Family USB Enhanced Host Controller #1");
                    break;
                case 0x1e2d:
                    i += sprintf(buf + i, "7 Series/C216 Chipset Family USB Enhanced Host Controller #2");
                    break;
                case 0x1e31:
                    i += sprintf(buf + i, "7 Series/C210 Series Chipset Family USB xHCI Host Controller");
                    break;
                case 0x1e3a:
                    i += sprintf(buf + i, "7 Series/C216 Chipset Family MEI Controller #1");
                    break;
                case 0x1e59:
                    i += sprintf(buf + i, "HM76 Express Chipset LPC Controller");
                    break;
                case 0x1237:
                    i += sprintf(buf + i, "440FX - 82441FX PMC [Natoma]");
                    break;
                case 0x2448:
                    i += sprintf(buf + i, "82801 Mobile PCI Bridge");
                    break;
                case 0x24c2:
                    i += sprintf(buf + i, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) USB UHCI Controller #1");
                    break;
                case 0x24c3:
                    i += sprintf(buf + i, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) SMBus Controller");
                    break;
                case 0x24c4:
                    i += sprintf(buf + i, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) USB UHCI Controller #2");
                    break;
                case 0x24c5:
                    i += sprintf(buf + i, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) AC'97 Audio Controller");
                    break;
                case 0x24c6:
                    i += sprintf(buf + i, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) AC'97 Modem Controller");
                    break;
                case 0x24c7:
                    i += sprintf(buf + i, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) USB UHCI Controller #3");
                    break;
                case 0x24ca:
                    i += sprintf(buf + i, "82801DBM (ICH4-M) IDE Controller");
                    break;
                case 0x24cc:
                    i += sprintf(buf + i, "82801DBM (ICH4-M) LPC Interface Bridge");
                    break;
                case 0x24cd:
                    i += sprintf(buf + i, "82801DB/DBM (ICH4/ICH4-M) USB2 EHCI Controller");
                    break;
                case 0x2922:
                    i += sprintf(buf + i, "82801IR/IO/IH (ICH9R/DO/DH) 6 port SATA Controller [AHCI mode]");
                    break;
                case 0x3340:
                    i += sprintf(buf + i, "82855PM Processor to I/O Controller");
                    break;
                case 0x3341:
                    i += sprintf(buf + i, "82855PM Processor to AGP Controller");
                    break;
                case 0x7000:
                    i += sprintf(buf + i, "82371SB PIIX3 ISA [Natoma/Triton II]");
                    break;
                case 0x7010:
                    i += sprintf(buf + i, "82371SB PIIX3 IDE [Natoma/Triton II]");
                    break;
                case 0x7113:
                    i += sprintf(buf + i, "82371AB/EB/MB PIIX4 ACPI");
                    break;
                case 0x7020:
                    i += sprintf(buf + i, "82371SB PIIX3 USB [Natoma/Triton II]");
                    break;
                default:
                    i += sprintf(buf + i, "<unknown>");
                    break;
            }
            break;
        case PCI_QEMU:
            i += sprintf(buf + i, "QEMU ");
            switch (device->device_id)
            {
                case 0x1111:
                    i += sprintf(buf + i, "Virtual Video Controller");
                    break;
                default:
                    i += sprintf(buf + i, "<unknown>");
                    break;
            }
            break;
        case PCI_QUALCOMM:
            i += sprintf(buf + i, "Qualcomm Atheros ");
            switch (device->device_id)
            {
                case 0x0013:
                    i += sprintf(buf + i, "AR5212/5213/2414 Wireless Network Adapter");
                    break;
                default:
                    i += sprintf(buf + i, "<unknown>");
                    break;
            }
            break;
        case PCI_VMWARE:
            i += sprintf(buf + i, "VMware ");
            switch (device->device_id)
            {
                case 0x0405:
                    i += sprintf(buf + i, "SVGA II Adapter");
                    break;
                default:
                    i += sprintf(buf + i, "<unknown>");
                    break;
            }
            break;
        default:
            i += sprintf(buf + i, "Device");
    }
    i += sprintf(buf + i, " [%04X:%04X]", device->vendor_id, device->device_id);

    return buf;
}

static inline char* pci_device_subsystem_description(char* buf, pci_device_t* device)
{
    int i = 0;
    switch (device->subsystem_vendor_id)
    {
        case 0x1014:
            i = sprintf(buf, "IBM");
            break;
        case 0x103c:
            i = sprintf(buf, "Hewlett-Packard Company");
            break;
        case 0x1af4:
            i = sprintf(buf, "QEMU Virtual Machine");
            break;
        default:
            i = sprintf(buf, "Unknown");
            break;
    }
    i += sprintf(buf + i, " [%04X:%04X]", device->subsystem_vendor_id, device->subsystem_id);
    return buf;
}

static inline char* pci_bar_description(char* buf, bar_t* bar)
{
    const char* unit = "B";
    uint32_t size = bar->size;

    if (bar->region == PCI_MEMORY)
    {
        human_size(size, unit);
        sprintf(buf, "memory %x [size=%u %s]", bar->addr, size, unit);
    }
    else
    {
        sprintf(buf, "io %x [size=%u]", bar->addr, size);
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
                pci_config_write_u32(bus, slot, func, addr, ~0UL);
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
    // FIXME: this can still overflow, some bound checking
    // is needed in below functions
    char description[256] = {0, };

    log_notice("%X:%X.%X %s",
        device->bus, device->slot, device->func,
        pci_device_description(description, device));

    description[0] = 0;
    log_notice("\tSubsystem: %s", pci_device_subsystem_description(description, device));
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
            log_notice("\tBAR%u: %s", i, pci_bar_description(description, bar));
        }
    }
}

void pci_devices_list(void)
{
    pci_device_t* device;

    list_for_each_entry(device, &pci_devices, list_entry)
    {
        pci_device_print(device);
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
