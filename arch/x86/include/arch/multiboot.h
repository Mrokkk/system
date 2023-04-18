#pragma once

#define MULTIBOOT_HEADER_MAGIC          0x1BADB002

#define MULTIBOOT_HEADER_FLAGS_ALIGNED  (1 << 0)
#define MULTIBOOT_HEADER_FLAGS_MEMORY   (1 << 1)
#define MULTIBOOT_HEADER_FLAGS_VIDEO    (1 << 2)
#define MULTIBOOT_HEADER_FLAGS_ADDRESS  (1 << 16)

#define MULTIBOOT_HEADER_FLAGS \
    MULTIBOOT_HEADER_FLAGS_ALIGNED | MULTIBOOT_HEADER_FLAGS_MEMORY | \
    MULTIBOOT_HEADER_FLAGS_VIDEO

// Magic number passed from Multiboot 1 compliant bootloader
#define MULTIBOOT_BOOTLOADER_MAGIC  0x2BADB002

#define MULTIBOOT_VIDEO_MODE_LIN    0
#define MULTIBOOT_VIDEO_MODE_EGA    1
#define MULTIBOOT_VIDEO_MODE_WIDTH  80
#define MULTIBOOT_VIDEO_MODE_HEIGHT 25

#ifndef __ASSEMBLER__

#include <stdint.h>

// The Multiboot header.
struct multiboot_header
{
    uint32_t magic;
    uint32_t flags;
    uint32_t checksum;
    uint32_t header_addr;
    uint32_t load_addr;
    uint32_t load_end_addr;
    uint32_t bss_end_addr;
    uint32_t entry_addr;
};

// The symbol table for a.out
struct aout_symbol_table
{
    uint32_t tabsize;
    uint32_t strsize;
    uint32_t addr;
    uint32_t reserved;
};

// The section header table for ELF
struct elf_section_header_table
{
    uint32_t num;
    uint32_t size;
    uint32_t addr;
    uint32_t shndx;
};

struct multiboot_info
{
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    union
    {
        struct aout_symbol_table aout_sym;
        struct elf_section_header_table elf_sec;
    };
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t bootloader_name;
    uint32_t apm_table;
    struct vbe_struct
    {
        uint32_t control_info;
        uint32_t mode_info;
        uint16_t mode;
        uint16_t interface_seg;
        uint16_t interface_off;
        uint16_t interface_len;
    } vbe;

    struct framebuffer
    {
        uint64_t addr;
        uint32_t pitch;
        uint32_t width;
        uint32_t height;
        uint8_t bpp;
#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED      0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB          1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT     2
        uint8_t type;

        union
        {
            struct
            {
                uint32_t palette_addr;
                uint16_t palette_num_colors;
            };
            struct
            {
                uint8_t red_field_position;
                uint8_t red_mask_size;
                uint8_t green_field_position;
                uint8_t green_mask_size;
                uint8_t blue_field_position;
                uint8_t blue_mask_size;
            };
        };
    } framebuffer;
};

struct module
{
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
};

struct memory_map
{
    uint32_t size;
    uint32_t base_addr_low;
    uint32_t base_addr_high;
    uint32_t length_low;
    uint32_t length_high;
#define MULTIBOOT_MEMORY_AVAILABLE              1
#define MULTIBOOT_MEMORY_RESERVED               2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE       3
#define MULTIBOOT_MEMORY_NVS                    4
#define MULTIBOOT_MEMORY_BADRAM                 5
    uint32_t type;
};

struct multiboot_drives_struct
{
    uint32_t size;
    uint8_t drive_number;
    uint8_t drive_mode;
    uint16_t drive_cylinders;
    uint8_t drive_heads;
    uint8_t drive_sectors;
    uint8_t drive_ports;
};

struct multiboot_apm_table_struct
{
    uint16_t version;
    uint16_t cseg;
    uint32_t offset;
    uint16_t cseg_16;
    uint16_t dseg;
    uint16_t flags;
    uint16_t cseg_len;
    uint16_t cseg_16_len;
    uint16_t dseg_len;
};

typedef struct modules_table
{
    struct module* modules;
    uint32_t count;
} modules_table_t;

#define MULTIBOOT_FLAGS_MEM_BIT             (1 << 0)
#define MULTIBOOT_FLAGS_BOOTDEV_BIT         (1 << 1)
#define MULTIBOOT_FLAGS_CMDLINE_BIT         (1 << 2)
#define MULTIBOOT_FLAGS_MODS_BIT            (1 << 3)
#define MULTIBOOT_FLAGS_SYMS_BIT            ((1 << 4) | (1 << 5))
#define MULTIBOOT_FLAGS_MMAP_BIT            (1 << 6)
#define MULTIBOOT_FLAGS_DRIVES_BIT          (1 << 7)
#define MULTIBOOT_FLAGS_CONFIG_TABLE_BIT    (1 << 8)
#define MULTIBOOT_FLAGS_BL_NAME_BIT         (1 << 9)
#define MULTIBOOT_FLAGS_APM_TABLE_BIT       (1 << 10)
#define MULTIBOOT_FLAGS_VBE_BIT             (1 << 11)
#define MULTIBOOT_FLAGS_FB_BIT              (1 << 12)

char* multiboot_read(struct multiboot_info *mb, uint32_t magic);
modules_table_t* modules_table_get();

extern uint32_t module_start;
extern uint32_t module_end;
extern struct framebuffer* framebuffer_ptr;
extern void* tux;
extern uint32_t tux_size;

#endif // __ASSEMBLER__
