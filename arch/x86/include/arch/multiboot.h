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
    unsigned long tabsize;
    unsigned long strsize;
    unsigned long addr;
    unsigned long reserved;
};

// The section header table for ELF
struct elf_section_header_table
{
    unsigned long num;
    unsigned long size;
    unsigned long addr;
    unsigned long shndx;
};

struct multiboot_info
{
    unsigned long flags;
    unsigned long mem_lower;
    unsigned long mem_upper;
    unsigned long boot_device;
    unsigned long cmdline;
    unsigned long mods_count;
    unsigned long mods_addr;
    union
    {
        struct aout_symbol_table aout_sym;
        struct elf_section_header_table elf_sec;
    };
    unsigned long mmap_length;
    unsigned long mmap_addr;
    unsigned long drives_length;
    unsigned long drives_addr;
    unsigned long config_table;
    unsigned long bootloader_name;
    unsigned long apm_table;
    struct vbe_struct
    {
        unsigned long control_info;
        unsigned long mode_info;
        unsigned short mode;
        unsigned short interface_seg;
        unsigned short interface_off;
        unsigned short interface_len;
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
    unsigned long mod_start;
    unsigned long mod_end;
    unsigned long string;
    unsigned long reserved;
};

// The memory map. Be careful that the offset 0 is base_addr_low but no size.
struct memory_map
{
    unsigned long size;
    unsigned long base_addr_low;
    unsigned long base_addr_high;
    unsigned long length_low;
    unsigned long length_high;
    unsigned long type;
};

struct multiboot_drives_struct
{
    unsigned int size;
    unsigned char drive_number;
    unsigned char drive_mode;
    unsigned short drive_cylinders;
    unsigned char drive_heads;
    unsigned char drive_sectors;
    unsigned char drive_ports;
};

struct multiboot_apm_table_struct
{
    unsigned short version;
    unsigned short cseg;
    unsigned int offset;
    unsigned short cseg_16;
    unsigned short dseg;
    unsigned short flags;
    unsigned short cseg_len;
    unsigned short cseg_16_len;
    unsigned short dseg_len;
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

char* multiboot_read(struct multiboot_info *mb, unsigned int magic);
modules_table_t* modules_table_get();

extern uint32_t module_start;
extern uint32_t module_end;
extern struct framebuffer* framebuffer_ptr;
extern void* tux;
extern uint32_t tux_size;

#endif // __ASSEMBLER__
