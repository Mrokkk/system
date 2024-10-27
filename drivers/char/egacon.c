#include "egacon.h"

#include <arch/io.h>

#include <kernel/mutex.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/string.h>

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
    uint16_t* videomem;
    size_t resx, resy;
    uint16_t cursor_offset;
    uint8_t default_attribute;
} data_t;

static inline void videomem_write(uint16_t* video_mem, uint16_t data, uint16_t offset)
{
    video_mem[offset] = data;
}

static inline uint16_t video_char_make(char c, char a)
{
    return ((a << 8) | c);
}

static inline void csr_move(uint16_t off)
{
    outb(14, 0x3d4);
    outb(off >> 8, 0x3d5);
    outb(15, 0x3d4);
    outb(off, 0x3d5);
}

static inline void cls(data_t* data)
{
    memsetw(data->videomem, video_char_make(' ', data->default_attribute), data->resx * data->resy);
    csr_move(0);
}

int egacon_init(console_driver_t* driver, size_t* row, size_t* col)
{
    data_t* data = alloc(data_t);

    if (!data)
    {
        log_warning("cannot allocate memory for egacon data");
        return -ENOMEM;
    }

    data->videomem = ptr(framebuffer.fb);
    data->default_attribute = forecolor(COLOR_GRAY) | backcolor(COLOR_BLACK);
    data->resx = framebuffer.width;
    data->resy = framebuffer.height;
    driver->data = data;
    cls(data);

    *row = framebuffer.height;
    *col = framebuffer.width;

    return 0;
}

void egacon_char_print(console_driver_t* drv, size_t row, size_t col, uint8_t c, uint32_t fgcolor, uint32_t bgcolor)
{
    data_t* data = drv->data;
    uint16_t off = row * data->resx + col;
    videomem_write(data->videomem, video_char_make(c, forecolor(fgcolor) | backcolor(bgcolor)), off);
}

void egacon_setsgr(console_driver_t*, uint32_t params[], size_t count, uint32_t* fgcolor, uint32_t* bgcolor)
{
    for (size_t i = 0; i < count; ++i)
    {
        uint32_t param = params[i];
        switch (param)
        {
            case 0:
                *fgcolor = COLOR_GRAY;
                *bgcolor = COLOR_BLACK;
                break;
            case 30: *fgcolor = COLOR_BLACK; break;
            case 31: *fgcolor = COLOR_RED; break;
            case 32: *fgcolor = COLOR_GREEN; break;
            case 33: *fgcolor = COLOR_YELLOW; break;
            case 34: *fgcolor = COLOR_BLUE; break;
            case 35: *fgcolor = COLOR_MAGENTA; break;
            case 36: *fgcolor = COLOR_CYAN; break;
            case 38:
                if (params[++i] == 2)
                {
                    // Unsupported
                    return;
                }
                break;
            case 40: *bgcolor = COLOR_BLACK; break;
            case 41: *bgcolor = COLOR_RED; break;
            case 42: *bgcolor = COLOR_GREEN; break;
            case 43: *bgcolor = COLOR_YELLOW; break;
            case 44: *bgcolor = COLOR_BLUE; break;
            case 45: *bgcolor = COLOR_MAGENTA; break;
            case 46: *bgcolor = COLOR_CYAN; break;
            case 90: *fgcolor = COLOR_GRAY; break;
            case 91: *fgcolor = COLOR_BRIGHTRED; break;
            case 92: *fgcolor = COLOR_BRIGHTGREEN; break;
            case 93: *fgcolor = COLOR_BRIGHTYELLOW; break;
            case 94: *fgcolor = COLOR_BRIGHTBLUE; break;
            case 95: *fgcolor = COLOR_BRIGHTMAGENTA; break;
            case 96: *fgcolor = COLOR_BRIGHTCYAN; break;
            case 97: *fgcolor = COLOR_BRIGHTWHITE; break;
        }
    }
}

void egacon_defcolor(console_driver_t*, uint32_t* fgcolor, uint32_t* bgcolor)
{
    *fgcolor = forecolor(COLOR_GRAY);
    *bgcolor = backcolor(COLOR_BLACK);
}

void egacon_movecsr(console_driver_t* drv, int x, int y)
{
    data_t* data = drv->data;
    uint16_t new_offset = y * data->resx + x;

    if (new_offset == data->cursor_offset)
    {
        return;
    }

    csr_move(new_offset);
    data->cursor_offset = new_offset;
}
