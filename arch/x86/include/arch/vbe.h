#pragma once

#include <stdint.h>
#include <kernel/kernel.h>

// Reference: https://web.archive.org/web/20081211174813/http://docs.ruudkoot.nl/vbe20.txt
// Reference: https://pdos.csail.mit.edu/6.828/2018/readings/hardware/vbe3.pdf
// Reference: https://www.versalogic.com/wp-content/themes/vsl-new/assets/resources/support/pdf/69030BG.pdf

struct vbe_info_block;
typedef struct vbe_info_block vbe_info_block_t;

struct vbe_mode_info_block;
typedef struct vbe_mode_info_block vbe_mib_t;

struct mode_info;
typedef struct mode_info mode_info_t;

struct mode_info
{
    union
    {
        struct { uint16_t resx, resy; };
        uint32_t is_valid;
    };

    uint16_t pitch;
    uint16_t mode;
    uint8_t bits:6;
    uint8_t type:1;
    uint8_t color_support:1;
    uint32_t framebuffer;
};

#define VBE_SUPPORTED   0x4f
#define VBE_SUCCESS     0x00
#define VBE_FAILURE     0x01
#define VBE_NOHWSUP     0x02
#define VBE_INVAL       0x03

#define VBE_GET_INFO_BLOCK  0x4f00
#define VBE_GET_MODE_INFO   0x4f01
#define VBE_SET_MODE        0x4f02
#define VBE_GET_MODE        0x4f03

#define EDID_SIGNATURE  0x00ffffffffffff00ULL

#define VBE_MODE_USE_LINEAR_FB  (1 << 14)

struct vbe_info_block
{
    char signature[4];
    uint16_t version;
    farptr_t oem_string;
    uint8_t capabilities[4];
    farptr_t video_mode;
    uint16_t total_memory;
    uint16_t oem_software_revision;
    farptr_t oem_vendor_name;
    farptr_t oem_product_name;
    farptr_t oem_product_revision;
    uint8_t reserved[222];
    uint8_t oem_strings[256];
} PACKED;

#define VBE_MODE_END            0xffff
#define VBE_MODE_SUPPORT(a)     (((a) >> 0) & 1)
#define VBE_MODE_COLOR(a)       (((a) >> 3) & 1)
#define VBE_MODE_TYPE(a)        (((a) >> 4) & 1)
#define VBE_MODE_TEXT           0
#define VBE_MODE_GRAPHICS       1
#define VBE_MODE_FRAMEBUFFER(a) (((a) >> 7) & 1)

struct vbe_mode_info_block
{
    uint16_t mode_attr;
    uint8_t wina_attr;
    uint8_t winb_attr;
    uint16_t win_granularity;
    uint16_t win_size;
    uint16_t wina_segment;
    uint16_t winb_segment;
    farptr_t win_func;
    uint16_t pitch;
    // VBE 1.2 and above
    uint16_t resx;
    uint16_t resy;
    uint8_t char_width;
    uint8_t char_heigth;
    uint8_t number_of_planes;
    uint8_t bits_per_pixel;
    uint8_t number_of_banks;
    uint8_t memory_model;
    uint8_t bank_size;
    uint8_t number_of_image_pages;
    uint8_t reserved[10];
    // VBE 2.0 and above
    uint32_t framebuffer;
    uint32_t off_screen_mem_off;
    uint16_t off_screen_mem_size;
} PACKED;

int vbe_initialize(void);
char* video_mode_print(char* buf, mode_info_t* m);
void video_modes_print(void);
