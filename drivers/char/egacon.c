#include "egacon.h"

#include <arch/io.h>

#include <kernel/page.h>
#include <kernel/mutex.h>
#include <kernel/string.h>
#include <kernel/module.h>
#include <kernel/device.h>

#include "console.h"
#include "framebuffer.h"

enum palette
{
    COLOR_BLACK         = 0,
    COLOR_RED           = 4,
    COLOR_GREEN         = 2,
    COLOR_YELLOW        = 6,
    COLOR_BLUE          = 1,
    COLOR_MAGENTA       = 5,
    COLOR_CYAN          = 3,
    COLOR_WHITE         = 15,
    COLOR_GRAY          = 7,
    COLOR_BRIGHTRED     = 12,
    COLOR_BRIGHTGREEN   = 10,
    COLOR_BRIGHTYELLOW  = 14,
    COLOR_BRIGHTBLUE    = 9,
    COLOR_BRIGHTMAGENTA = 13,
    COLOR_BRIGHTCYAN    = 11,
    COLOR_BRIGHTWHITE   = 15,
};

#define forecolor(x)        (x)
#define backcolor(x)        ((x << 4) & 0x7f)
#define blink               (1 << 7)

typedef struct
{
    uint16_t* fb;
    size_t resx, resy;
    uint8_t default_attribute;
    uint8_t fg_color;
    uint8_t bg_color;
} driver_data_t;

static driver_data_t data;

static inline void video_mem_write(uint16_t* video_mem, uint16_t data, uint16_t offset)
{
    video_mem[offset] = data;
}

static inline uint16_t make_video_char(char c, char a)
{
    return ((a << 8) | c);
}

static inline void csr_move(uint16_t off)
{
    outb(14, 0x3D4);
    outb(off >> 8, 0x3D5);
    outb(15, 0x3D4);
    outb(off, 0x3D5);
}

static inline void cls(uint16_t* video_mem)
{
    uint16_t blank = 0x20 | (data.default_attribute << 8);

    memsetw(video_mem, blank, data.resx * data.resy);
    csr_move(0);
}

int egacon_init(struct console_driver* driver, size_t* row, size_t* col)
{
    driver->data = &data;
    data.fb = virt_ptr(framebuffer.fb);
    data.default_attribute = forecolor(COLOR_GRAY) | backcolor(COLOR_BLACK);
    data.fg_color = COLOR_GRAY;
    data.bg_color = COLOR_BLACK;
    data.resx = framebuffer.width;
    data.resy = framebuffer.height;
    cls(data.fb);
    *row = framebuffer.height;
    *col = framebuffer.width;
    return 0;
}

void egacon_char_print(
    struct console_driver*,
    size_t row,
    size_t col,
    unsigned int c)
{
    uint16_t off = row * data.resx + col;
    uint16_t* video_mem = data.fb;
    video_mem_write(video_mem, make_video_char(c, forecolor(data.fg_color) | backcolor(data.bg_color)), off);
    csr_move(off + 1);
}

void egacon_setsgr(console_driver_t*, uint32_t params[], size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        uint32_t param = params[i];
        switch (param)
        {
            case 0:
                data.fg_color = COLOR_GRAY;
                data.bg_color = COLOR_BLACK;
                break;
            case 30: data.fg_color = COLOR_BLACK; break;
            case 31: data.fg_color = COLOR_RED; break;
            case 32: data.fg_color = COLOR_GREEN; break;
            case 33: data.fg_color = COLOR_YELLOW; break;
            case 34: data.fg_color = COLOR_BLUE; break;
            case 35: data.fg_color = COLOR_MAGENTA; break;
            case 36: data.fg_color = COLOR_CYAN; break;
            case 38:
                if (params[++i] == 2)
                {
                    // Unsupported
                    return;
                }
                break;
            case 40: data.bg_color = COLOR_BLACK; break;
            case 41: data.bg_color = COLOR_RED; break;
            case 42: data.bg_color = COLOR_GREEN; break;
            case 43: data.bg_color = COLOR_YELLOW; break;
            case 44: data.bg_color = COLOR_BLUE; break;
            case 45: data.bg_color = COLOR_MAGENTA; break;
            case 46: data.bg_color = COLOR_CYAN; break;
            case 90: data.fg_color = COLOR_GRAY; break;
            case 91: data.fg_color = COLOR_BRIGHTRED; break;
            case 92: data.fg_color = COLOR_BRIGHTGREEN; break;
            case 93: data.fg_color = COLOR_BRIGHTYELLOW; break;
            case 94: data.fg_color = COLOR_BRIGHTBLUE; break;
            case 95: data.fg_color = COLOR_BRIGHTMAGENTA; break;
            case 96: data.fg_color = COLOR_BRIGHTCYAN; break;
            case 97: data.fg_color = COLOR_BRIGHTWHITE; break;
        }
    }
}
