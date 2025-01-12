#include "egacon.h"

#include <arch/io.h>

#include <kernel/mutex.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/page_alloc.h>

#include "font.h"
#include "glyph.h"
#include "framebuffer.h"

static void egacon_glyph_draw(console_driver_t* driver, size_t x, size_t y, glyph_t* glyph);
static void egacon_sgr_16(console_driver_t* driver, uint8_t value, uint32_t* color);
static void egacon_sgr_8(console_driver_t* driver, uint8_t value, uint32_t* color);
static void egacon_defcolor(console_driver_t* driver, uint32_t* fgcolor, uint32_t* bgcolor);
static int egacon_font_load(console_driver_t* driver, const void* buffer, size_t size, size_t* resx, size_t* resy);

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
    uint8_t line[32];
} vga_glyph_t;

typedef struct
{
    uint16_t*    videomem;
    vga_glyph_t* vga_font;
    uint8_t      default_attribute;
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

static int egacon_font_load_internal(console_driver_t* drv, font_t* font, size_t* resx, size_t* resy)
{
    int errno;
    data_t* data = drv->data;

    scoped_irq_lock();

    if (font->width != 8 || font->height != 16)
    {
        errno = -EINVAL;
        goto failure;
    }

    if (!data->vga_font)
    {
        data->vga_font = mmio_map(0xa0000, 256 * 32, "vgafont");

        if (unlikely(!data->vga_font))
        {
            errno = -ENOMEM;
            goto failure;
        }
    }

    outw(0x0005, 0x3ce);
    outw(0x0406, 0x3ce);
    outw(0x0402, 0x3c4);
    outw(0x0604, 0x3c4);

    void* font_glyphs = font->glyphs;

    for (size_t i = 0; i < font->glyphs_count; ++i, font_glyphs += font->bytes_per_glyph)
    {
        memcpy(data->vga_font[i].line, font_glyphs, font->bytes_per_glyph);
        memset(data->vga_font[i].line + font->bytes_per_glyph, 0, 32 - font->bytes_per_glyph);
    }

    memset(data->vga_font[font->glyphs_count].line, 0, font->bytes_per_glyph * (256 - font->glyphs_count));

    *resx = framebuffer.width;
    *resy = framebuffer.height;

    font_unload(font);

    outw(0x0302, 0x3c4);
    outw(0x0204, 0x3c4);
    outw(0x1005, 0x3ce);
    outw(0x0e06, 0x3ce);

    return 0;

failure:
    font_unload(font);
    return errno;
}

int egacon_init(console_driver_t* driver, console_config_t* config, size_t* resx, size_t* resy)
{
    font_t* font;
    data_t* data;

    data = alloc(data_t);

    if (!data)
    {
        log_warning("cannot allocate memory for egacon data");
        return -ENOMEM;
    }

    data->vga_font          = NULL;
    data->videomem          = ptr(framebuffer.fb);
    data->default_attribute = forecolor(COLOR_GRAY) | backcolor(COLOR_BLACK);
    driver->data            = data;

    if (!font_load_from_file(config->font_path, &font))
    {
        egacon_font_load_internal(driver, font, resx, resy);
    }
    else
    {
        *resx = framebuffer.width;
        *resy = framebuffer.height;
    }

    driver->glyph_draw = &egacon_glyph_draw;
    driver->defcolor   = &egacon_defcolor;
    driver->sgr_16     = &egacon_sgr_16;
    driver->sgr_8      = &egacon_sgr_8;
    driver->font_load  = &egacon_font_load;

    cls(data);

    outb(0x0a, 0x3d4);
    outb(0x20, 0x3d5);

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

static int egacon_font_load(console_driver_t* drv, const void* buffer, size_t size, size_t* resx, size_t* resy)
{
    int errno;
    font_t* font = NULL;

    if (unlikely(errno = font_load_from_buffer(buffer, size, &font)))
    {
        return errno;
    }

    return egacon_font_load_internal(drv, font, resx, resy);
}
