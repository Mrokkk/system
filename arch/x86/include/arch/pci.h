#pragma once

#include <stdint.h>
#include <kernel/list.h>

// Reference for PCI BIOS: http://www.o3one.org/hwdocs/bios_doc/pci_bios_21.pdf

typedef struct pci_device pci_device_t;

enum pci_registers
{
    PCI_CONFIG_ADDRESS = 0xcf8,
    PCI_CONFIG_DATA    = 0xcfc,
};

enum pci_header_type
{
    PCI_HEADER_DEVICE         = 0,
    PCI_HEADER_PCI_BRIDGE     = 1,
    PCI_HEADER_CARDBUS_BRIDGE = 2,
};

enum pci_register_offset
{
    PCI_REG_VENDOR_DEVICE_ID = 0x00,
    PCI_REG_COMMAND          = 0x04,
    PCI_REG_HEADER_TYPE      = 0x0e,
    PCI_REG_BAR0             = 0x10,
    PCI_REG_BAR1             = 0x14,
    PCI_REG_BAR2             = 0x18,
    PCI_REG_BAR3             = 0x1c,
    PCI_REG_BAR4             = 0x20,
    PCI_REG_BAR5             = 0x24,
    PCI_REG_BAR_END          = 0x28,
    PCI_REG_HEADER0_END      = 0x40,
};

enum pci_interrupt
{
    PCI_INTERRUPT_PIN_UNUSED   = 0x00,
    PCI_INTERRUPT_LINE_INVALID = 0xff,
};

enum pci_command
{
    PCI_COMMAND_IO     = 1 << 0,
    PCI_COMMAND_MEMORY = 1 << 1,
    PCI_COMMAND_BM     = 1 << 2,
};

enum pci_space
{
    PCI_32BIT = 0,
    PCI_1MIB  = 1,
    PCI_64BIT = 2,
};

enum pci_region
{
    PCI_MEMORY = 0,
    PCI_IO     = 1,
};

struct base_address_register
{
    uint32_t addr;
    uint32_t size;
    uint8_t region;
    uint8_t space;
};

typedef struct base_address_register bar_t;

enum pci_cap_id
{
    PCI_CAP_ID_AGP          = 0x02,
    PCI_CAP_ID_MSI          = 0x05,
    PCI_CAP_ID_VNDR         = 0x09,
    PCI_CAP_ID_MSIX         = 0x0b,
};

struct pci_cap
{
    uint8_t id;
    uint8_t next;
    uint8_t len;
    uint8_t data;
};

typedef struct pci_cap pci_cap_t;

enum pci_msi
{
    MSI_BASE_ADDRESS      = 0xfee00000,
    MSI_MSG_CTRL_ENABLE   = (1 << 0),
    MSI_MSG_CTRL_64BIT    = (1 << 7),
    MSI_MSG_CTRL_MASKABLE = (1 << 8),
};

struct pci_msi_cap
{
    uint8_t id;
    uint8_t next;
    uint16_t msg_ctrl;
    union
    {
        struct
        {
            uint32_t msg_addr;
            uint16_t msg_data;
            uint16_t reserved;
            uint32_t mask;
            uint32_t pending;
        } b32;
        struct
        {
            uint64_t msg_addr;
            uint16_t msg_data;
            uint16_t reserved;
            uint32_t mask;
            uint32_t pending;
        } b64;
    };
};

typedef struct pci_msi_cap pci_msi_cap_t;

struct pci_device
{
    uint16_t    vendor_id;
    uint16_t    device_id;
    uint16_t    command;
    uint16_t    status;
    uint8_t     revision_id;
    uint8_t     prog_if;
    uint8_t     subclass;
    uint8_t     class;
    uint8_t     cacheline_size;
    uint8_t     latency_timer;
    uint8_t     header_type:7;
    uint8_t     multi_function:1;
    uint8_t     bist;
    union
    {
        struct
        {
            bar_t       bar[6];
            uint32_t    cis;
            uint16_t    subsystem_vendor_id;
            uint16_t    subsystem_id;
            uint32_t    rom_base;
            uint8_t     capabilities;
            uint8_t     reserved[7];
            uint8_t     interrupt_line;
            uint8_t     interrupt_pin;
            uint8_t     min_grant;
            uint8_t     max_latency;
        };
        struct
        {
            bar_t       bar[2];
            uint8_t     primary_bus;
            uint8_t     secondary_bus;
            uint8_t     subordinate_bus;
            uint8_t     secondary_latency_timer;
            uint8_t     io_base;
            uint8_t     io_limit;
            uint16_t    secondary_status;
            uint16_t    memory_base;
            uint16_t    memory_limit;
            uint32_t    reserved[3];
            uint16_t    io_base_hi;
            uint16_t    io_limit_hi;
            uint8_t     capabilities;
            uint8_t     reserved_2[3];
            uint32_t    rom_base;
            uint8_t     interrupt_line;
            uint8_t     interrupt_pin;
            uint16_t    bridge_control;
        } pci_bridge;
        struct
        {

        } cardbus_bridge;
    };
    uint8_t     bus, slot, func;
    void*       private;
    list_head_t list_entry;
};

enum pci_class
{
    PCI_UNCLASSIFIED  = 0x0,
    PCI_STORAGE       = 0x1,
    PCI_NETWORK       = 0x2,
    PCI_DISPLAY       = 0x3,
    PCI_MULTIMEDIA    = 0x4,
    PCI_BRIDGE        = 0x6,
    PCI_COMCONTROLLER = 0x7,
    PCI_SERIAL_BUS    = 0xc,
};

enum pci_storage_subclass
{
    PCI_STORAGE_SCSI = 0,
    PCI_STORAGE_IDE  = 1,
    PCI_STORAGE_SATA = 6,
};

enum pci_display_subclass
{
    PCI_DISPLAY_VGA        = 0,
    PCI_DISPLAY_XGA        = 1,
    PCI_DISPLAY_3D         = 2,
    PCI_DISPLAY_CONTROLLER = 0x80,
};

enum pci_multimedia_subclass
{
    PCI_MULTIMEDIA_VIDEO_CONTROLLER = 0,
    PCI_MULTIMEDIA_AUDIO_CONTROLLER = 1,
    PCI_MULTIMEDIA_AUTIO_DEVICE     = 3,
};

enum pci_bridge_subclass
{
    PCI_BRIDGE_HOST    = 0,
    PCI_BRIDGE_ISA     = 1,
    PCI_BRIDGE_EISA    = 2,
    PCI_BRIDGE_MCA     = 3,
    PCI_BRIDGE_PCI     = 4,
    PCI_BRIDGE_CARDBUS = 7,
    PCI_BRIDGE_OTHER   = 0x80,
};

enum pci_serial_bus_subclass
{
    PCI_SERIAL_BUS_USB = 3,
};

enum pci_vendor
{
    PCI_AMD            = 0x1002,
    PCI_TI             = 0x104c,
    PCI_VIRTIO         = 0x1af4,
    PCI_REDHAT         = 0x1b36,
    PCI_INTEL          = 0x8086,
    PCI_QEMU           = 0x1234,
    PCI_VMWARE         = 0x15ad,
    PCI_QUALCOMM       = 0x168c,
    PCI_CIRRUS         = 0x1013,
    PCI_VIA            = 0x1106,
    PCI_3DFX           = 0x121a,
    PCI_S3             = 0x5333,
    PCI_ESONIQ         = 0x1274,
    PCI_VENDOR_INVALID = 0xffff,
};

static inline bool pci_interrupt_valid(pci_device_t* device)
{
    return device->interrupt_pin != PCI_INTERRUPT_PIN_UNUSED
        && device->interrupt_line != PCI_INTERRUPT_LINE_INVALID;
}

void pci_scan(void);

int pci_device_initialize(pci_device_t* device);

uint32_t pci_config_readl(pci_device_t* device, uint8_t offset, int* status);
uint16_t pci_config_readw(pci_device_t* device, uint8_t offset, int* status);
uint8_t pci_config_readb(pci_device_t* device, uint8_t offset, int* status);

// Below functions will always write 4-bytes aligned uint32_t. When offset is
// unaligned it reads 4 or 8 bytes (depending on length and alignment), replaces
// proper bytes and writes it back
int pci_config_writel(pci_device_t* device, uint8_t offset, uint32_t value);
int pci_config_writew(pci_device_t* device, uint8_t offset, uint32_t value);
int pci_config_writeb(pci_device_t* device, uint8_t offset, uint32_t value);

// Below require 4-bytes aligned offset and 4-bytes aligned size
int pci_config_read(pci_device_t* device, uint8_t offset, void* buffer, size_t size);
int pci_config_write(pci_device_t* device, uint8_t offset, const void* buffer, size_t size);

int pci_cap_find(pci_device_t* device, uint8_t id, void* buffer, size_t size);
void pci_device_print(pci_device_t* device);
pci_device_t* pci_device_get(uint8_t class, uint8_t subclass);
pci_device_t* pci_device_get_at(uint8_t bus, uint8_t slot, uint8_t func);
void pci_device_enumerate(void (*probe)(pci_device_t* device, void* data), void* data);
void pci_device_describe(pci_device_t* device, char** vendor_id, char** device_id);
