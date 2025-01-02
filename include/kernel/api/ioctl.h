#pragma once

#include <stddef.h>

#define KDSETMODE       0x4b3a  // set text/graphics mode
#define     KD_TEXT     0x00
#define     KD_GRAPHICS 0x01
#define KDGETMODE       0x4b3b  // get current mode

#define TCGETA              0x4400
#define TCSETA              0x4401
#define TIOCGETA            0x4500
#define TIOCGWINSZ          0x4501
#define TIOCSWINSZ          0x4502
#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602

typedef struct winsize winsize_t;
typedef struct fb_var_screeninfo fb_var_screeninfo_t;

struct fb_var_screeninfo
{
    size_t xres;
    size_t yres;
    size_t bits_per_pixel;
    size_t pitch;
};

struct winsize
{
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

int ioctl(int fd, unsigned long request, ...);
