#pragma once

#include <stdint.h>
#include <kernel/list.h>

// Reference for PCI BIOS: http://www.o3one.org/hwdocs/bios_doc/pci_bios_21.pdf

struct pci_device;
typedef struct pci_device pci_device_t;

#define PCI_CONFIG_ADDRESS  0xcf8
#define PCI_CONFIG_DATA     0xcfc

typedef enum
{
    DEVICE = 0,
    BRIDGE = 1,
} header_type_t;

enum register_offset
{
    VENDOR_DEVICE_ID    = 0x00,
    HEADER_TYPE         = 0x0e,
    BAR0                = 0x10,
    BAR1                = 0x14,
    BAR2                = 0x18,
    BAR3                = 0x1C,
    BAR4                = 0x20,
    BAR5                = 0x24,
    BAR_END             = 0x28,
    HEADER0_END         = 0x40,
};

enum pci_space
{
    PCI_32BIT   = 0,
    PCI_1MIB    = 1,
    PCI_64BIT   = 2,
};

enum pci_region
{
    PCI_MEMORY  = 0,
    PCI_IO      = 1,
};

struct base_address_register
{
    uint32_t addr;
    uint32_t size;
    uint8_t region;
    uint8_t space;
};

typedef struct base_address_register bar_t;

struct pci_device
{
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_id;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t class;
    uint8_t cacheline_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
    bar_t bar[6];
    uint32_t cis;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint32_t rom_base;
    uint8_t capabilities[4];
    uint32_t reserved;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint8_t min_grant;
    uint8_t max_latency;
    uint8_t bus, slot, func, padding;
    list_head_t list_entry;
};

enum class
{
    PCI_UNCLASSIFIED    = 0x0,
    PCI_STORAGE         = 0x1,
    PCI_NETWORK         = 0x2,
    PCI_DISPLAY         = 0x3,
    PCI_MULTIMEDIA      = 0x4,
    PCI_BRIDGE          = 0x6,
    PCI_COMCONTROLLER   = 0x7,
    PCI_SERIAL_BUS      = 0xc,
};

enum storage_subclass
{
    PCI_STORAGE_SCSI    = 0,
    PCI_STORAGE_IDE     = 1,
    PCI_STORAGE_SATA    = 6,
};

static inline const char* storage_subclass_string(int c)
{
    switch (c)
    {
        case PCI_STORAGE_SCSI: return "SCSI storage controller";
        case PCI_STORAGE_IDE: return "IDE interface";
        case PCI_STORAGE_SATA: return "SATA controller";
        default: return "Unknown";
    }
}

enum display_subclass
{
    PCI_DISPLAY_VGA     = 0,
    PCI_DISPLAY_XGA     = 1,
    PCI_DISPLAY_3D      = 2,
    PCI_DISPLAY_CONTROLLER = 0x80,
};

static inline const char* display_subclass_string(int c)
{
    switch (c)
    {
        case PCI_DISPLAY_VGA: return "VGA compatible controller";
        case PCI_DISPLAY_XGA: return "XGA compatible controller";
        case PCI_DISPLAY_3D: return "3D controller";
        case PCI_DISPLAY_CONTROLLER: return "Display controller";
        default: return "Unknown";
    }
}

enum multimedia_subclass
{
    PCI_MULTIMEDIA_VIDEO_CONTROLLER     = 0,
    PCI_MULTIMEDIA_AUDIO_CONTROLLER     = 1,
    PCI_MULTIMEDIA_AUTIO_DEVICE         = 3,
};

static inline const char* multimedia_subclass_string(int c)
{
    switch (c)
    {
        case PCI_MULTIMEDIA_VIDEO_CONTROLLER: return "Multimedia video controller";
        case PCI_MULTIMEDIA_AUDIO_CONTROLLER: return "Multimedia audio controller";
        case PCI_MULTIMEDIA_AUTIO_DEVICE: return "Audio device";
        default: return "Unknown";
    }
}

enum bridge_subclass
{
    PCI_BRIDGE_HOST     = 0,
    PCI_BRIDGE_ISA      = 1,
    PCI_BRIDGE_EISA     = 2,
    PCI_BRIDGE_MCA      = 3,
    PCI_BRIDGE_PCI      = 4,
    PCI_BRIDGE_CARDBUS  = 7,
    PCI_BRIDGE_OTHER    = 0x80,
};

static inline const char* bridge_subclass_string(int c)
{
    switch (c)
    {
        case PCI_BRIDGE_HOST: return "Host bridge";
        case PCI_BRIDGE_ISA: return "ISA bridge";
        case PCI_BRIDGE_EISA: return "EISA bridge";
        case PCI_BRIDGE_MCA: return "MCA bridge";
        case PCI_BRIDGE_PCI: return "PCI bridge";
        case PCI_BRIDGE_CARDBUS: return "CardBus bridge";
        case PCI_BRIDGE_OTHER: return "Bridge";
        default: return "Unknown bridge";
    }
}

enum serial_bus_subclass
{
    PCI_SERIAL_BUS_USB = 3,
};

static inline const char* serial_bus_subclass_string(int c)
{
    switch (c)
    {
        case PCI_SERIAL_BUS_USB: return "USB controller";
        default: return "Serial bus controller";
    }
}

enum pci_vendor
{
    PCI_AMD         = 0x1002,
    PCI_TI          = 0x104c,
    PCI_VIRTIO      = 0x1af4,
    PCI_INTEL       = 0x8086,
    PCI_QEMU        = 0x1234,
    PCI_VMWARE      = 0x15ad,
    PCI_QUALCOMM    = 0x168c,
};

void pci_scan(void);
uint16_t pci_config_read_u16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint32_t pci_config_read_u32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_config_write_u32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
void pci_device_print(pci_device_t* device);
pci_device_t* pci_device_get(uint8_t class, uint8_t subclass);
