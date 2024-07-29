#pragma once

// Reference: https://www.gnu.org/software/grub/manual/multiboot/multiboot.html

#define MULTIBOOT_HEADER_MAGIC          0x1badb002

#define MULTIBOOT_HEADER_FLAGS_ALIGNED  (1 << 0)
#define MULTIBOOT_HEADER_FLAGS_MEMORY   (1 << 1)
#define MULTIBOOT_HEADER_FLAGS_VIDEO    (1 << 2)
#define MULTIBOOT_HEADER_FLAGS_ADDRESS  (1 << 16)

#define MULTIBOOT_HEADER_VIDEO_MODE_LIN 0
#define MULTIBOOT_HEADER_VIDEO_MODE_EGA 1

// Magic number passed from Multiboot 1 compliant bootloader
#define MULTIBOOT_BOOTLOADER_MAGIC  0x2badb002

#ifndef __ASSEMBLER__

#include <stdarg.h>
#include <stdint.h>

struct multiboot_info;
typedef struct multiboot_info multiboot_info_t;

struct module;
typedef struct module multiboot_module_t;

struct framebuffer
{
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t type;
#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED      0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB          1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT     2
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
    uint32_t unused[9];
    uint32_t bootloader_name;
};

struct module
{
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
};

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

char* multiboot_read(va_list args);

extern struct framebuffer* framebuffer_ptr;
extern void* disk_img;
extern void* ksyms_module;
extern void* ksyms_start;
extern void* ksyms_end;

#endif // __ASSEMBLER__
