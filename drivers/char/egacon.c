#include "egacon.h"

#include <arch/io.h>

#include <kernel/mutex.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/string.h>

#include "glyph.h"
#include "framebuffer.h"

static void egacon_glyph_draw(console_driver_t* driver, size_t x, size_t y, glyph_t* glyph);
static void egacon_sgr_16(console_driver_t* driver, uint8_t value, uint32_t* color);
static void egacon_sgr_8(console_driver_t* driver, uint8_t value, uint32_t* color);
static void egacon_defcolor(console_driver_t* driver, uint32_t* fgcolor, uint32_t* bgcolor);

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
    uint8_t   default_attribute;
} data_t;

static inline void videomem_write(uint16_t* video_mem, uint16_t data, uint16_t offset)
{
    video_mem[offset] = data;
}

static inline uint16_t video_char_make(char c, char a)
{
    return ((a << 8) | c);
}

static inline void cls(data_t* data)
{
    memsetw(data->videomem, video_char_make(' ', data->default_attribute), framebuffer.width * framebuffer.height);
}

int egacon_init(console_driver_t* driver, console_config_t*, size_t* resx, size_t* resy)
{
    data_t* data = alloc(data_t);

    if (!data)
    {
        log_warning("cannot allocate memory for egacon data");
        return -ENOMEM;
    }

    data->videomem          = ptr(framebuffer.fb);
    data->default_attribute = forecolor(COLOR_GRAY) | backcolor(COLOR_BLACK);

    driver->data       = data;
    driver->glyph_draw = &egacon_glyph_draw;
    driver->defcolor   = &egacon_defcolor;
    driver->sgr_16     = &egacon_sgr_16;
    driver->sgr_8      = &egacon_sgr_8;

    cls(data);

    outb(0x0a, 0x3d4);
    outb(0x20, 0x3d5);

    *resx = framebuffer.width;
    *resy = framebuffer.height;

    return 0;
}

static void egacon_glyph_draw(console_driver_t* drv, size_t x, size_t y, glyph_t* glyph)
{
    data_t* data = drv->data;
    uint16_t off = y * framebuffer.width + x;
    uint32_t fgcolor;
    uint32_t bgcolor;

    if (glyph->attr & GLYPH_ATTR_INVERSED)
    {
        fgcolor = glyph->bgcolor;
        bgcolor = glyph->fgcolor;
    }
    else
    {
        fgcolor = glyph->fgcolor;
        bgcolor = glyph->bgcolor;
    }

    videomem_write(
        data->videomem,
        video_char_make(glyph->c, forecolor(fgcolor) | backcolor(bgcolor)),
        off);
}

static void egacon_color_set(uint8_t value, uint32_t* color)
{
    switch (value)
    {
        case 0: *color = COLOR_BLACK; break;
        case 1: *color = COLOR_RED; break;
        case 2: *color = COLOR_GREEN; break;
        case 3: *color = COLOR_YELLOW; break;
        case 4: *color = COLOR_BLUE; break;
        case 5: *color = COLOR_MAGENTA; break;
        case 6: *color = COLOR_CYAN; break;
        case 7: *color = COLOR_WHITE; break;
        case 8: *color = COLOR_GRAY; break;
        case 9: *color = COLOR_BRIGHTRED; break;
        case 10: *color = COLOR_BRIGHTGREEN; break;
        case 11: *color = COLOR_BRIGHTYELLOW; break;
        case 12: *color = COLOR_BRIGHTBLUE; break;
        case 13: *color = COLOR_BRIGHTMAGENTA; break;
        case 14: *color = COLOR_BRIGHTCYAN; break;
        case 15: *color = COLOR_BRIGHTWHITE; break;
    }
}

static void egacon_sgr_16(console_driver_t*, uint8_t value, uint32_t* color)
{
    egacon_color_set(value, color);
}

static void egacon_sgr_8(console_driver_t*, uint8_t value, uint32_t* color)
{
    egacon_color_set(value, color);
}

static void egacon_defcolor(console_driver_t*, uint32_t* fgcolor, uint32_t* bgcolor)
{
    *fgcolor = forecolor(COLOR_GRAY);
    *bgcolor = backcolor(COLOR_BLACK);
}
