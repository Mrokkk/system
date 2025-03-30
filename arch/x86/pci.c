#include <arch/io.h>
#include <arch/pci.h>
#include <arch/bios32.h>
#include <kernel/init.h>
#include <kernel/kernel.h>

static LIST_DECLARE(pci_devices);

#define PCI_BIOS_DISABLED 1

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

#define PCI_MULTIFUNCTION  0x80
#define PCI_BUS_COUNT      32
#define PCI_SLOT_COUNT     32
#define PCI_FUNCTION_COUNT 8
#define PCI_NO_DEVICE      0xffff

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

static uint32_t pci_config_readl_impl(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    outl(pci_io_address(bus, slot, func, offset), PCI_CONFIG_ADDRESS);
    return inl(PCI_CONFIG_DATA);
}

static void pci_config_writel_impl(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val)
{
    outl(pci_io_address(bus, slot, func, offset), PCI_CONFIG_ADDRESS);
    outl(val, PCI_CONFIG_DATA);
}

static void pci_command_write(pci_device_t* device, uint16_t command)
{
    pci_config_writel_impl(device->bus, device->slot, device->func, PCI_REG_COMMAND, command);
    device->command = pci_config_readl_impl(device->bus, device->slot, device->func, PCI_REG_COMMAND) & 0xffff;
}

#define STATUS_SET(x) \
    if (status) *status = (x)

uint32_t pci_config_readl(pci_device_t* device, uint8_t offset, int* status)
{
    if (unlikely(offset + 4 < offset))
    {
        STATUS_SET(-EINVAL);
        return 0;
    }

    uint8_t dword_offset = (offset & 3) * 8;
    offset &= ~3;

    if (!dword_offset)
    {
        STATUS_SET(0);
        return pci_config_readl_impl(device->bus, device->slot, device->func, offset);
    }

    uint32_t data = pci_config_readl_impl(device->bus, device->slot, device->func, offset) >> dword_offset;
    data |= pci_config_readl_impl(device->bus, device->slot, device->func, offset + 4) << (32 - dword_offset);

    STATUS_SET(0);

    return data;
}

uint16_t pci_config_readw(pci_device_t* device, uint8_t offset, int* status)
{
    if (unlikely(offset + 2 < offset))
    {
        STATUS_SET(-EINVAL);
        return 0;
    }

    uint8_t dword_offset = (offset & 3) * 8;
    offset &= ~3;

    if (dword_offset < 24)
    {
        STATUS_SET(0);
        return (pci_config_readl_impl(device->bus, device->slot, device->func, offset) >> dword_offset) & 0xffff;
    }

    uint32_t data = (pci_config_readl_impl(device->bus, device->slot, device->func, offset) >> dword_offset) & 0xff;
    data |= (pci_config_readl_impl(device->bus, device->slot, device->func, offset + 4) & 0xff) << 8;

    STATUS_SET(0);

    return data;
}

uint8_t pci_config_readb(pci_device_t* device, uint8_t offset, int* status)
{
    uint8_t dword_offset = (offset & 3) * 8;

    STATUS_SET(0);

    return (pci_config_readl_impl(device->bus, device->slot, device->func, offset) >> dword_offset) & 0xff;
}

int pci_config_writel(pci_device_t* device, uint8_t offset, uint32_t value)
{
    if (unlikely(offset + 4 < offset))
    {
        return -EINVAL;
    }

    uint8_t dword_offset = (offset & 3) * 8;
    offset &= ~3;

    if (!dword_offset)
    {
        pci_config_writel_impl(device->bus, device->slot, device->func, offset, value);
        return 0;
    }

    uint32_t data = pci_config_readl_impl(device->bus, device->slot, device->func, offset);

    data &= 0xffffffff >> dword_offset;
    data |= value << dword_offset;

    pci_config_writel_impl(device->bus, device->slot, device->func, offset, data);

    data = pci_config_readl_impl(device->bus, device->slot, device->func, offset + 4);

    data &= 0xffffffff << (32 - dword_offset);
    data |= value >> (32 - dword_offset);

    pci_config_writel_impl(device->bus, device->slot, device->func, offset, data);

    return 0;
}

int pci_config_writew(pci_device_t* device, uint8_t offset, uint32_t value)
{
    if (unlikely(offset + 2 < offset))
    {
        return -EINVAL;
    }

    uint8_t dword_offset = (offset & 3) * 8;
    offset &= ~3;

    uint32_t data = pci_config_readl_impl(device->bus, device->slot, device->func, offset);

    uint32_t mask = ~(0xffff << dword_offset);
    data &= mask;
    data |= value << dword_offset;

    pci_config_writel_impl(device->bus, device->slot, device->func, offset, data);

    if (dword_offset == 3)
    {
        data = pci_config_readl_impl(device->bus, device->slot, device->func, offset + 4);

        data &= 0xffffffff << (32 - dword_offset);
        data |= value >> (32 - dword_offset);

        pci_config_writel_impl(device->bus, device->slot, device->func, offset, data);
    }

    return 0;
}

int pci_config_writeb(pci_device_t* device, uint8_t offset, uint32_t value)
{
    uint8_t dword_offset = (offset & 3) * 8;
    offset &= ~3;

    uint32_t data = pci_config_readl_impl(device->bus, device->slot, device->func, offset);

    uint32_t mask = ~(0xff << dword_offset);
    data &= mask;
    data |= value << dword_offset;

    pci_config_writel_impl(device->bus, device->slot, device->func, offset, data);

    return 0;
}

int pci_device_initialize(pci_device_t* device)
{
    pci_command_write(device, device->command | PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_BM);
    return !(device->command & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_BM));
}

int pci_config_read(pci_device_t* device, uint8_t offset, void* buffer, size_t size)
{
    uint32_t* buf32 = buffer;

    if (unlikely(size % 4 || offset % 4))
    {
        log_error("unaligned read of %u to %#04x", size, offset);
        return -1;
    }

    for (; size >= 4; size -= 4, buf32++, offset += 4)
    {
        *buf32 = pci_config_readl_impl(device->bus, device->slot, device->func, offset);
    }

    return 0;
}

int pci_config_write(pci_device_t* device, uint8_t offset, const void* buffer, size_t size)
{
    const uint32_t* buf32 = buffer;

    if (unlikely(size % 4 || offset % 4))
    {
        log_error("unaligned write of %u to %#04x", size, offset);
        return -1;
    }

    for (; size >= 4; size -= 4, buf32++, offset += 4)
    {
        pci_config_writel_impl(device->bus, device->slot, device->func, offset, *buf32);
    }

    return 0;
}

int pci_cap_find(pci_device_t* device, uint8_t id, void* buffer, size_t size)
{
    pci_cap_t* cap = buffer;

    if (unlikely(size < sizeof(*cap) || size % 4))
    {
        return -EINVAL;
    }

    for (uint8_t ptr = device->capabilities; ptr; ptr = cap->next)
    {
        if (unlikely(pci_config_read(device, ptr, buffer, sizeof(*cap))))
        {
            log_warning("cannot read CAP at %#x", ptr);
            continue;
        }

        if (cap->id == id)
        {
            if (size > sizeof(pci_cap_t))
            {
                pci_config_read(device, ptr, buffer + sizeof(*cap), size - sizeof(*cap));

                return ptr;
            }
        }
    }

    return -ENOENT;
}

static void pci_device_add(uint8_t bus, uint8_t slot, uint8_t func)
{
    pci_device_t* device = zalloc(pci_device_t);

    if (unlikely(!device))
    {
        log_error("cannot allocate pci_device");
        return;
    }

    list_init(&device->list_entry);
    device->bus  = bus;
    device->slot = slot;
    device->func = func;

    uint32_t* buf = ptr(device);

    for (uint8_t addr = 0; addr < PCI_REG_BAR0; addr += 4)
    {
        *buf++ = pci_config_readl_impl(bus, slot, func, addr);
    }

    mb();

    if (device->header_type == PCI_HEADER_DEVICE || device->header_type == PCI_HEADER_PCI_BRIDGE)
    {
        uint32_t last_bar = device->header_type == PCI_HEADER_DEVICE
            ? PCI_REG_BAR_END
            : PCI_REG_BAR2;

        uint32_t command = pci_config_readl_impl(bus, slot, func, PCI_REG_COMMAND);
        pci_config_writel_impl(bus, slot, func, PCI_REG_COMMAND, command & ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY));

        for (uint8_t addr = PCI_REG_BAR0; addr < last_bar; addr += 4)
        {
            bar_t* bar = ptr(buf);
            uint32_t raw_bar = pci_config_readl_impl(bus, slot, func, addr);

            if (raw_bar)
            {
                pci_config_writel_impl(bus, slot, func, addr, ~0);

                bar->size = ~(pci_config_readl_impl(bus, slot, func, addr) & ~0xf) + 1;
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
                    bar->size &= 0xffff;
                }

                pci_config_writel_impl(bus, slot, func, addr, raw_bar);
            }
            buf = ptr(addr(buf) + sizeof(bar_t));
        }

        pci_config_writel_impl(bus, slot, func, PCI_REG_COMMAND, command);

        for (uint32_t addr = last_bar; addr < PCI_REG_HEADER0_END; addr += 4)
        {
            *buf++ = pci_config_readl_impl(bus, slot, func, addr);
        }
    }
    else
    {
        for (uint32_t addr = PCI_REG_BAR0; addr < PCI_REG_HEADER0_END; addr += 4)
        {
            *buf++ = pci_config_readl_impl(bus, slot, func, addr);
        }
    }

    mb();

    if (device->header_type == PCI_HEADER_DEVICE)
    {
        device->capabilities &= ~3;
    }

    list_add_tail(&device->list_entry, &pci_devices);
}

UNMAP_AFTER_INIT void pci_scan(void)
{
    uint32_t bus, slot, func;
    uint32_t bus_size = PCI_BUS_COUNT;

#ifdef __i386__
    if (!PCI_BIOS_DISABLED && !bios32_find(PCI_BIOS_SIGNATURE, &pci_bios_entry))
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
        for (slot = 0; slot < PCI_SLOT_COUNT; ++slot)
        {
            uint16_t vendor_id = pci_config_readl_impl(bus, slot, 0, PCI_REG_VENDOR_DEVICE_ID) & 0xffff;

            if (vendor_id == PCI_NO_DEVICE)
            {
                continue;
            }

            pci_device_add(bus, slot, 0);

            uint8_t header_type = (pci_config_readl_impl(bus, slot, 0, 12) >> 16) & 0xff;

            if (header_type & PCI_MULTIFUNCTION)
            {
                for (func = 1; func < PCI_FUNCTION_COUNT; ++func)
                {
                    vendor_id = pci_config_readl_impl(bus, slot, func, PCI_REG_VENDOR_DEVICE_ID) & 0xffff;

                    if (vendor_id != PCI_NO_DEVICE)
                    {
                        pci_device_add(bus, slot, func);
                    }
                }
            }
        }
    }

    param_call_if_set(KERNEL_PARAM("pciprint"), &pci_devices_list);
}

static inline const char* storage_subclass_string(int c)
{
    switch (c)
    {
        case PCI_STORAGE_SCSI: return "SCSI storage controller";
        case PCI_STORAGE_IDE:  return "IDE interface";
        case PCI_STORAGE_SATA: return "SATA controller";
        default:               return "Unknown";
    }
}

static inline const char* display_subclass_string(int c)
{
    switch (c)
    {
        case PCI_DISPLAY_VGA:        return "VGA compatible controller";
        case PCI_DISPLAY_XGA:        return "XGA compatible controller";
        case PCI_DISPLAY_3D:         return "3D controller";
        case PCI_DISPLAY_CONTROLLER: return "Display controller";
        default:                     return "Unknown";
    }
}

static inline const char* multimedia_subclass_string(int c)
{
    switch (c)
    {
        case PCI_MULTIMEDIA_VIDEO_CONTROLLER: return "Multimedia video controller";
        case PCI_MULTIMEDIA_AUDIO_CONTROLLER: return "Multimedia audio controller";
        case PCI_MULTIMEDIA_AUTIO_DEVICE:     return "Audio device";
        default:                              return "Unknown";
    }
}

static inline const char* bridge_subclass_string(int c)
{
    switch (c)
    {
        case PCI_BRIDGE_HOST:    return "Host bridge";
        case PCI_BRIDGE_ISA:     return "ISA bridge";
        case PCI_BRIDGE_EISA:    return "EISA bridge";
        case PCI_BRIDGE_MCA:     return "MCA bridge";
        case PCI_BRIDGE_PCI:     return "PCI bridge";
        case PCI_BRIDGE_CARDBUS: return "CardBus bridge";
        case PCI_BRIDGE_OTHER:   return "Bridge";
        default:                 return "Unknown bridge";
    }
}

static inline const char* serial_bus_subclass_string(int c)
{
    switch (c)
    {
        case PCI_SERIAL_BUS_USB: return "USB controller";
        default:                 return "Serial bus controller";
    }
}

#define DEVICE_ID(id, name) \
    case id: if (device_id) { *device_id = name; }; break

#define UNKNOWN_DEVICE_ID() \
    default: if (device_id) { *device_id = "<unknown>"; }; break

#define VENDOR_ID(id, name) \
    case id: \
        if (vendor_id) { *vendor_id = name; }; \
        switch (device->device_id)

#define UNKNOWN_VENDOR_ID() \
    default: \
        if (vendor_id) { *vendor_id = "Device"; }; \
        if (device_id) { *device_id = "<unknown>"; }; \
        break

void pci_device_describe(pci_device_t* device, char** vendor_id, char** device_id)
{
    switch (device->vendor_id)
    {
        VENDOR_ID(PCI_AMD, "Advanced Micro Devices, Inc. [AMD/ATI]")
        {
            DEVICE_ID(0x4c4d, "Rage Mobility AGP 2x Series");
            DEVICE_ID(0x4c57, "RV200/M7 [Mobility Radeon 7500]");
            DEVICE_ID(0x4e50, "RV350/M10 / RV360/M11 [Mobility Radeon 9600 (PRO) / 9700]");
            DEVICE_ID(0x5046, "Rage 4 [Rage 128 PRO AGP 4X]");
            UNKNOWN_DEVICE_ID();
        }
        break;

        VENDOR_ID(PCI_TI, "Texas Instruments")
        {
            DEVICE_ID(0xac46, "PCI4520 PC card Cardbus Controller");
            DEVICE_ID(0xac55, "PCI1520 PC card Cardbus Controller");
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

        VENDOR_ID(PCI_REDHAT, "Red Hat, Inc.")
        {
            DEVICE_ID(0x000d, "QEMU XHCI Host Controller");
            UNKNOWN_DEVICE_ID();
        }
        break;

        VENDOR_ID(PCI_INTEL, "Intel")
        {
            DEVICE_ID(0x0154, "3rd Gen Core processor DRAM Controller");
            DEVICE_ID(0x0166, "3rd Gen Core processor Graphics Controller");
            DEVICE_ID(0x100e, "82540EM Gigabit Ethernet Controller");
            DEVICE_ID(0x101e, "82540EP Gigabit Ethernet Controller (Mobile)");
            DEVICE_ID(0x1031, "82801CAM (ICH3) PRO/100 VE (LOM) Ethernet Controller");
            DEVICE_ID(0x10d3, "82574L Gigabit Network Connection");
            DEVICE_ID(0x1503, "82579V Gigabit Network Connection");
            DEVICE_ID(0x1a30, "82845 845 [Brookdale] Chipset Host Bridge");
            DEVICE_ID(0x1a31, "82845 845 [Brookdale] Chipset AGP Bridge");
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
            DEVICE_ID(0x2415, "82801AA AC'97 Audio Controller");
            DEVICE_ID(0x2448, "82801 Mobile PCI Bridge");
            DEVICE_ID(0x2482, "82801CA/CAM USB Controller #1");
            DEVICE_ID(0x2483, "82801CA/CAM SMBus Controller");
            DEVICE_ID(0x2484, "82801CA/CAM USB Controller #2");
            DEVICE_ID(0x2485, "82801CA/CAM AC'97 Audio Controller");
            DEVICE_ID(0x2486, "82801CA/CAM AC'97 Modem Controller");
            DEVICE_ID(0x2487, "82801CA/CAM USB Controller #3");
            DEVICE_ID(0x248a, "82801CAM IDE U100 Controller");
            DEVICE_ID(0x248c, "82801CAM ISA Bridge (LPC)");
            DEVICE_ID(0x24c2, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) USB UHCI Controller #1");
            DEVICE_ID(0x24c3, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) SMBus Controller");
            DEVICE_ID(0x24c4, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) USB UHCI Controller #2");
            DEVICE_ID(0x24c5, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) AC'97 Audio Controller");
            DEVICE_ID(0x24c6, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) AC'97 Modem Controller");
            DEVICE_ID(0x24c7, "82801DB/DBL/DBM (ICH4/ICH4-L/ICH4-M) USB UHCI Controller #3");
            DEVICE_ID(0x24ca, "82801DBM (ICH4-M) IDE Controller");
            DEVICE_ID(0x24cc, "82801DBM (ICH4-M) LPC Interface Bridge");
            DEVICE_ID(0x24cd, "82801DB/DBM (ICH4/ICH4-M) USB2 EHCI Controller");
            DEVICE_ID(0x2918, "82801IB (ICH9) LPC Interface Controller");
            DEVICE_ID(0x2922, "82801IR/IO/IH (ICH9R/DO/DH) 6 port SATA Controller [AHCI mode]");
            DEVICE_ID(0x2930, "82801I (ICH9 Family) SMBus Controller");
            DEVICE_ID(0x2934, "82801I (ICH9 Family) USB UHCI Controller #1");
            DEVICE_ID(0x2935, "82801I (ICH9 Family) USB UHCI Controller #2");
            DEVICE_ID(0x2936, "82801I (ICH9 Family) USB UHCI Controller #3");
            DEVICE_ID(0x2937, "82801I (ICH9 Family) USB UHCI Controller #4");
            DEVICE_ID(0x2938, "82801I (ICH9 Family) USB UHCI Controller #5");
            DEVICE_ID(0x2939, "82801I (ICH9 Family) USB UHCI Controller #6");
            DEVICE_ID(0x293a, "82801I (ICH9 Family) USB2 EHCI Controller #1");
            DEVICE_ID(0x293c, "82801I (ICH9 Family) USB2 EHCI Controller #2");
            DEVICE_ID(0x29c0, "82G33/G31/P35/P31 Express DRAM Controller");
            DEVICE_ID(0x3340, "82855PM Processor to I/O Controller");
            DEVICE_ID(0x3341, "82855PM Processor to AGP Controller");
            DEVICE_ID(0x7000, "82371SB PIIX3 ISA [Natoma/Triton II]");
            DEVICE_ID(0x7010, "82371SB PIIX3 IDE [Natoma/Triton II]");
            DEVICE_ID(0x7110, "82371AB/EB/MB PIIX4 ISA");
            DEVICE_ID(0x7111, "82371AB/EB/MB PIIX4 IDE");
            DEVICE_ID(0x7112, "82371AB/EB/MB PIIX4 USB");
            DEVICE_ID(0x7113, "82371AB/EB/MB PIIX4 ACPI");
            DEVICE_ID(0x7180, "440LX/EX - 82443LX/EX Host bridge");
            DEVICE_ID(0x7181, "440LX/EX - 82443LX/EX AGP bridge");
            DEVICE_ID(0x7190, "440BX/ZX/DX - 82443BX/ZX/DX Host bridge");
            DEVICE_ID(0x7191, "440BX/ZX/DX - 82443BX/ZX/DX AGP bridge");
            DEVICE_ID(0x71a0, "440GX - 82443GX Host bridge");
            DEVICE_ID(0x71a1, "440GX - 82443GX AGP bridge");
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

        VENDOR_ID(PCI_VIA, "VIA Technologies, Inc.")
        {
            DEVICE_ID(0x0571, "VT82C586A/B/VT82C686/A/B/VT823x/A/C PIPC Bus Master IDE");
            DEVICE_ID(0x0596, "VT82C596 ISA [Mobile South]");
            DEVICE_ID(0x3038, "VT82xx/62xx/VX700/8x0/900 UHCI USB 1.1 Controller");
            DEVICE_ID(0x3050, "VT82C596 Power Management");
            DEVICE_ID(0x0691, "VT82C693A/694x [Apollo PRO133x]");
            DEVICE_ID(0x8598, "VT82C598/694x [Apollo MVP3/Pro133x AGP]");
            DEVICE_ID(0x3057, "VT82C686 [Apollo Super ACPI]");
            DEVICE_ID(0x0686, "VT82C686 [Apollo Super South]");
            UNKNOWN_DEVICE_ID();
        }
        break;

        VENDOR_ID(PCI_3DFX, "3Dfx Interactive, Inc.")
        {
            DEVICE_ID(0x0001, "Voodoo");
            DEVICE_ID(0x0002, "Voodoo 2");
            DEVICE_ID(0x0003, "Voodoo Banshee");
            DEVICE_ID(0x0004, "Voodoo Banshee [Velocity 100]");
            DEVICE_ID(0x0005, "Voodoo 3");
            UNKNOWN_DEVICE_ID();
        }
        break;

        VENDOR_ID(PCI_S3, "S3 Graphics Ltd.")
        {
            DEVICE_ID(0x5631, "86c325 [ViRGE]");
            UNKNOWN_DEVICE_ID();
        }
        break;

        VENDOR_ID(PCI_ESONIQ, "Ensoniq")
        {
            DEVICE_ID(0x1371, "ES1371/ES1373 / Creative Labs CT2518");
            UNKNOWN_DEVICE_ID();
        }
        break;

        UNKNOWN_VENDOR_ID();
    }
}

static char* pci_device_description(pci_device_t* device, char* buf, size_t size)
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

    char* vendor_id = NULL;
    char* device_id = NULL;

    pci_device_describe(device, &vendor_id, &device_id);

    it = csnprintf(it, end, " [%02x%02x]: %s %s [%04x:%04x]",
        device->class,
        device->subclass,
        vendor_id,
        device_id,
        device->vendor_id,
        device->device_id);

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
        SUBSYSTEM_VENDOR_ID(0x121a, "3Dfx Interactive, Inc.");
        SUBSYSTEM_VENDOR_ID(0x8086, "Intel");
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

static uint32_t pci_bridge_io(uint16_t low, uint16_t hi)
{
    return ((low & ~(0xf)) << 8) | (hi << 16);
}

void pci_device_print(pci_device_t* device)
{
    char description[256] = {0, };

    log_notice("%02x:%02x.%x %s",
        device->bus, device->slot, device->func,
        pci_device_description(device, description, sizeof(description)));

    log_notice("  Header:  %#x;    Status:  %#06x",
        device->header_type,
        device->status);

    log_notice("  Command: %#06x; Prog IF: %#04x",
        device->command,
        device->prog_if);

    switch (device->header_type)
    {
        case PCI_HEADER_DEVICE:
            if (pci_interrupt_valid(device))
            {
                log_notice("  Interrupt: INT%c# IRQ%u", device->interrupt_pin - 1 + 'A', device->interrupt_line);
            }
            for (size_t i = 0; i < array_size(device->bar); ++i)
            {
                bar_t* bar = device->bar + i;
                if (!bar->addr)
                {
                    continue;
                }
                log_notice("  BAR%u: %s", i, pci_bar_description(bar, description, sizeof(description)));
            }
            if (device->rom_base)
            {
                log_notice("  Expansion ROM address: %#x", device->rom_base);
            }
            if (device->subsystem_id || device->subsystem_vendor_id)
            {
                log_notice("  Subsystem: %s", pci_device_subsystem_description(device, description, sizeof(description)));
            }
            break;
        case PCI_HEADER_PCI_BRIDGE:
            if (pci_interrupt_valid(device))
            {
                log_notice("  Interrupt: INT%c# IRQ %u", device->pci_bridge.interrupt_pin - 1 + 'A', device->pci_bridge.interrupt_line);
            }
            for (size_t i = 0; i < array_size(device->pci_bridge.bar); ++i)
            {
                bar_t* bar = device->pci_bridge.bar + i;
                if (!bar->addr)
                {
                    continue;
                }
                log_notice("  BAR%u: %s", i, pci_bar_description(bar, description, sizeof(description)));
            }
            log_notice("  Primary/secondary/subordinate bus: %02x/%02x/%02x",
                device->pci_bridge.primary_bus,
                device->pci_bridge.secondary_bus,
                device->pci_bridge.subordinate_bus);

            log_notice("  IO range: [%#x - %#x]",
                pci_bridge_io(device->pci_bridge.io_base, device->pci_bridge.io_base_hi),
                pci_bridge_io(device->pci_bridge.io_limit, device->pci_bridge.io_limit_hi) | 0xfff);

            if (device->pci_bridge.memory_limit)
            {
                log_notice("  Memory range: [%#x - %#x]", device->pci_bridge.memory_base << 16, device->pci_bridge.memory_limit << 16);
            }

            if (device->pci_bridge.rom_base)
            {
                log_notice("  Expansion ROM address: %#x", device->pci_bridge.rom_base);
            }
            break;
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

pci_device_t* pci_device_get_at(uint8_t bus, uint8_t slot, uint8_t func)
{
    pci_device_t* device;
    list_for_each_entry(device, &pci_devices, list_entry)
    {
        if (device->bus == bus && device->slot == slot && device->func == func)
        {
            return device;
        }
    }
    return NULL;
}

void pci_device_enumerate(void (*probe)(pci_device_t* device, void* data), void* data)
{
    pci_device_t* device;
    list_for_each_entry(device, &pci_devices, list_entry)
    {
        probe(device, data);
    }
}
