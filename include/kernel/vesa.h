#pragma once

#include <arch/vbe.h>

// References:
// https://web.archive.org/web/20081211174813/http://docs.ruudkoot.nl/vbe20.txt
// https://pdos.csail.mit.edu/6.828/2018/readings/hardware/vbe3.pdf
// https://www.versalogic.com/wp-content/themes/vsl-new/assets/resources/support/pdf/69030BG.pdf

#define VBE_SUPPORTED       0x4f
#define VBE_SUCCESS         0x00
#define VBE_FAILURE         0x01
#define VBE_NOHWSUP         0x02
#define VBE_INVAL           0x03

#define VBE_GET_INFO_BLOCK  0x4f00
#define VBE_GET_MODE_INFO   0x4f01
#define VBE_SET_MODE        0x4f02
#define VBE_GET_MODE        0x4f03

#define EDID_SIGNATURE      0x00ffffffffffff00ULL

#define VBE_MODE_END                    0xffff
#define VBE_MODE_SUPPORT(a)             (((a) >> 0) & 1)
#define VBE_MODE_COLOR(a)               (((a) >> 3) & 1)
#define VBE_MODE_TYPE(a)                (((a) >> 4) & 1)
#define VBE_MODE_WINMEMORY(a)           (((a) >> 5) & 1)
#define VBE_MODE_TEXT                   0
#define VBE_MODE_GRAPHICS               1
#define VBE_MODE_FRAMEBUFFER(a)         (((a) >> 7) & 1)
#define VBE_MODE_LINEAR_FB              (1 << 14)
#define VBE_MODE_MEMORY_MODEL_TEXT      0x00
#define VBE_MODE_MEMORY_MODEL_CGA       0x01
#define VBE_MODE_MEMORY_MODEL_HERCULES  0x02
#define VBE_MODE_MEMORY_MODEL_PLANAR    0x03
#define VBE_MODE_MEMORY_MODEL_PACKED    0x04
#define VBE_MODE_MEMORY_MODEL_NONCHAIN  0x05
#define VBE_MODE_MEMORY_MODEL_DIRECT    0x06
#define VBE_MODE_MEMORY_MODEL_YUV       0x07

#define VBE_PMI_GET(regs) \
    ({ \
        memset(&regs, 0, sizeof(regs)); \
        regs.ax = 0x4f0a; \
        &regs; \
    })

#define VBE_MODE_GET(regs) \
    ({ \
        memset(&regs, 0, sizeof(regs)); \
        regs.ax = VBE_GET_INFO_BLOCK; \
        regs.di = VBE_INFO_BLOCK_ADDR; \
        &regs; \
    })

#define VBE_EDID_GET(regs) \
    ({ \
        memset(&regs, 0, sizeof(regs)); \
        regs.ax = 0x4f15; \
        regs.bl = 0x01; \
        regs.cx = 0x00; \
        regs.dx = 0x00; \
        regs.di = VBE_EDID_ADDR; \
        &regs; \
    })

typedef struct vbe_info_block vbe_info_block_t;
typedef struct vbe_mode_info_block vbe_mib_t;

struct vbe_info_block
{
    char     signature[4];
    uint16_t version;
    farptr_t oem_string;
    uint8_t  capabilities[4];
    farptr_t video_mode;
    uint16_t total_memory;
    uint16_t oem_software_revision;
    farptr_t oem_vendor_name;
    farptr_t oem_product_name;
    farptr_t oem_product_revision;
    uint8_t  reserved[222];
    uint8_t  oem_strings[256];
} PACKED;

struct vbe_mode_info_block
{
    union
    {
        uint16_t value;
        struct
        {
            uint8_t supported:1;
            uint8_t reserved1:1;
            uint8_t tty_output:1;
            uint8_t color:1;
            uint8_t type:1;
            uint8_t vga:1;
            uint8_t no_win_mem_mode:1;
            uint8_t linear_framebuffer:1;
        };
    } mode_attr;
    uint8_t  wina_attr;
    uint8_t  winb_attr;
    uint16_t win_granularity;
    uint16_t win_size;
    uint16_t wina_segment;
    uint16_t winb_segment;
    farptr_t win_func;
    uint16_t pitch;
    // VBE 1.2 and above
    uint16_t resx;
    uint16_t resy;
    uint8_t  char_width;
    uint8_t  char_heigth;
    uint8_t  number_of_planes;
    uint8_t  bits_per_pixel;
    uint8_t  number_of_banks;
    uint8_t  memory_model;
    uint8_t  bank_size;
    uint8_t  number_of_image_pages;
    uint8_t  reserved;
    uint8_t  red_mask_size;
    uint8_t  red_field_position;
    uint8_t  green_mask_size;
    uint8_t  green_field_position;
    uint8_t  blue_mask_size;
    uint8_t  blue_field_position;
    uint8_t  rsvd_mask_size;
    uint8_t  rsvd_field_position;
    uint8_t  direct_color_mode_info;
    // VBE 2.0 and above
    uint32_t framebuffer;
    uint32_t reserved2;
    uint16_t reserved3;
    uint8_t  reserved4[206];
};

int vbe_call(vbe_regs_t* regs);
