#pragma once

#include <stdint.h>
#include <stddef.h>

#define KDSETMODE       0x4b3a  // set text/graphics mode
#define     KD_TEXT     0x00
#define     KD_GRAPHICS 0x01
#define KDGETMODE       0x4b3b  // get current mode
#define KDFONTOP        0x4b72

#define TCGETA              0x4400
#define TCSETA              0x4401
#define TIOCGETA            0x4500
#define TIOCGWINSZ          0x4501
#define TIOCSWINSZ          0x4502
#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602

#define FB_TYPE_PACKED_PIXELS           0 // Packed Pixels
#define FB_TYPE_PLANES                  1 // Non interleaved planes
#define FB_TYPE_INTERLEAVED_PLANES      2 // Interleaved planes
#define FB_TYPE_TEXT                    3 // Text/attributes
#define FB_TYPE_VGA_PLANES              4 // EGA/VGA planes

#define FB_AUX_TEXT_MDA                 0 // Monochrome text
#define FB_AUX_TEXT_CGA                 1 // CGA/EGA/VGA Color text
#define FB_AUX_TEXT_S3_MMIO             2 // S3 MMIO fasttext
#define FB_AUX_TEXT_MGA_STEP16          3 // MGA Millenium I: text, attr, 14 reserved bytes
#define FB_AUX_TEXT_MGA_STEP8           4 // other MGAs:      text, attr,  6 reserved bytes

#define FB_AUX_VGA_PLANES_VGA4          0 // 16 color planes (EGA/VGA)
#define FB_AUX_VGA_PLANES_CFB4          1 // CFB4 in planes (VGA)
#define FB_AUX_VGA_PLANES_CFB8          2 // CFB8 in planes (VGA)

#define FB_VISUAL_MONO01                0 // Monochr. 1=Black 0=White
#define FB_VISUAL_MONO10                1 // Monochr. 1=White 0=Black
#define FB_VISUAL_TRUECOLOR             2 // True color
#define FB_VISUAL_PSEUDOCOLOR           3 // Pseudo color (like atari)
#define FB_VISUAL_DIRECTCOLOR           4 // Direct color
#define FB_VISUAL_STATIC_PSEUDOCOLOR    5 // Pseudo color readonly

#define FB_ACCEL_NONE                   0 // No hardware accelerator

typedef struct winsize winsize_t;
typedef struct console_font_op console_font_op_t;
typedef struct fb_fix_screeninfo fb_fix_screeninfo_t;
typedef struct fb_var_screeninfo fb_var_screeninfo_t;

struct fb_fix_screeninfo
{
    char      id[16];
    uintptr_t smem_start;
    size_t    smem_len;
    int       type;
    int       type_aux;
    int       visual;
    size_t    line_length;
    int       accel;
};

struct fb_var_screeninfo
{
    size_t xres;
    size_t yres;
    size_t bits_per_pixel;
};

struct console_font_op
{
    void*  data;
    size_t size;
};

struct winsize
{
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

int ioctl(int fd, unsigned long request, ...);
