#pragma once

#include <stdint.h>

#define KDSETMODE       0x4b3a  // set text/graphics mode
#define     KD_TEXT     0x00
#define     KD_GRAPHICS 0x01
#define KDGETMODE       0x4b3b  // get current mode

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602

struct fb_var_screeninfo;
typedef struct fb_var_screeninfo fb_var_screeninfo_t;

struct fb_var_screeninfo
{
    uint32_t xres;
    uint32_t yres;
    uint32_t bits_per_pixel;
    uint32_t pitch;
};

int ioctl(int fd, unsigned long request, ...);
