#include "fbcon.h"

#include <kernel/kernel.h>

#include "font.h"
#include "glyph.h"
#include "framebuffer.h"
#include "console_driver.h"

#define DEBUG_FBCON             0
#define GLYPH_OFFSET_HORIZONTAL 0

static void fbcon_glyph_draw(console_driver_t* driver, size_t y, size_t x, glyph_t* glyph);
static void fbcon_sgr_rgb(console_driver_t* driver, uint32_t value, uint32_t* color);
static void fbcon_sgr_256(console_driver_t* driver, uint32_t value, uint32_t* color);
static void fbcon_sgr_16(console_driver_t* driver, uint8_t value, uint32_t* color);
static void fbcon_sgr_8(console_driver_t* driver, uint8_t value, uint32_t* color);
static void fbcon_defcolor(console_driver_t* driver, uint32_t* fgcolor, uint32_t* bgcolor);
static int fbcon_font_load(console_driver_t*, const void* buffer, size_t size, size_t* resx, size_t* resy);

enum palette
{
#if 0
    COLOR_BLACK         = 0x000000,
    COLOR_RED           = 0xcd0000,
    COLOR_GREEN         = 0x00cd00,
    COLOR_YELLOW        = 0xcdcd00,
    COLOR_BLUE          = 0x006fb8,
    COLOR_MAGENTA       = 0xcd00cd,
    COLOR_CYAN          = 0x00cdcd,
    COLOR_WHITE         = 0x999999,
    COLOR_GRAY          = 0x7f7f7f,
    COLOR_BRIGHTBLACK   = COLOR_GRAY,
    COLOR_BRIGHTRED     = 0xff0000,
    COLOR_BRIGHTGREEN   = 0x00ff00,
    COLOR_BRIGHTYELLOW  = 0xffff00,
    COLOR_BRIGHTBLUE    = 0x0000ff,
    COLOR_BRIGHTMAGENTA = 0xff00ff,
    COLOR_BRIGHTCYAN    = 0x00ffff,
    COLOR_BRIGHTWHITE   = 0xffffff,
#else
    COLOR_BLACK         = 0x282828,
    COLOR_RED           = 0xea6962,
    COLOR_GREEN         = 0xa9b665,
    COLOR_YELLOW        = 0xd8a657,
    COLOR_BLUE          = 0x7daea3,
    COLOR_MAGENTA       = 0xd3869b,
    COLOR_CYAN          = 0x89b482,
    COLOR_WHITE         = 0xd6d6d6,
    COLOR_GRAY          = 0x3c3836,
    COLOR_BRIGHTBLACK   = COLOR_GRAY,
    COLOR_BRIGHTRED     = 0xea6962,
    COLOR_BRIGHTGREEN   = 0xa9b665,
    COLOR_BRIGHTYELLOW  = 0xd8a657,
    COLOR_BRIGHTBLUE    = 0x7daea3,
    COLOR_BRIGHTMAGENTA = 0xd3869b,
    COLOR_BRIGHTCYAN    = 0x89b482,
    COLOR_BRIGHTWHITE   = 0xd4be98,
#endif
};

typedef enum
{
    NORMAL,
    COLOR256,
    RGB,
} color_mode_t;

typedef struct
{
    uint8_t* fb;
    font_t*  font;
    size_t   pitch;
    size_t   width;
    size_t   font_height_offset;
    size_t   font_width_offset;
    size_t   font_bytes_per_line;
    uint32_t mask;
} data_t;

static uint32_t palette[] = {
    COLOR_BLACK,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_BLUE,
    COLOR_MAGENTA,
    COLOR_CYAN,
    COLOR_WHITE,
    COLOR_BRIGHTBLACK,
    COLOR_BRIGHTRED,
    COLOR_BRIGHTGREEN,
    COLOR_BRIGHTYELLOW,
    COLOR_BRIGHTBLUE,
    COLOR_BRIGHTMAGENTA,
    COLOR_BRIGHTCYAN,
    COLOR_BRIGHTWHITE,
};

static void fbcon_font_set(data_t* data, font_t* font)
{
    data->font = font;
    data->width = font->width + GLYPH_OFFSET_HORIZONTAL;
    data->font_height_offset = framebuffer.pitch * font->height;
    data->font_width_offset = (framebuffer.bpp / 8) * data->width;
    data->font_bytes_per_line = font->bytes_per_line;
    data->mask = 1 << (font->bytes_per_line * 8 - 1);
}

static void fbcon_size_set(data_t* data, size_t* resx, size_t* resy)
{
    *resx = framebuffer.width / data->width;
    *resy = framebuffer.height / data->font->height;
}

int fbcon_init(console_driver_t* driver, console_config_t* config, size_t* resx, size_t* resy)
{
    int errno;
    data_t* data;
    font_t* font;

    if (unlikely(errno = font_load_from_file(config->font_path, &font)))
    {
        log_warning("cannot load font: %d", errno);
        return errno;
    }

    data = alloc(data_t);

    if (unlikely(!data))
    {
        log_warning("cannot allocate memory for fbcon data");
        return -ENOMEM;
    }

    data->fb = framebuffer.fb;
    data->pitch = framebuffer.pitch;

    fbcon_font_set(data, font);

    driver->data       = data;
    driver->glyph_draw = &fbcon_glyph_draw;
    driver->defcolor   = &fbcon_defcolor;
    driver->font_load  = &fbcon_font_load;
    driver->sgr_rgb    = &fbcon_sgr_rgb;
    driver->sgr_256    = &fbcon_sgr_256;
    driver->sgr_16     = &fbcon_sgr_16;
    driver->sgr_8      = &fbcon_sgr_8;

    fbcon_size_set(data, resx, resy);

    return 0;
}

static void fbcon_glyph_draw(console_driver_t* drv, size_t x, size_t y, glyph_t* glyph)
{
    data_t* data = drv->data;
    font_t* font = data->font;
    uint32_t line, mask;
    uint8_t c = glyph->c;
    uint32_t fgcolor;
    uint32_t bgcolor;
    uint32_t def_mask = data->mask;

    if (glyph->attr & GLYPH_ATTR_INVERSED)
    {
        bgcolor = glyph->fgcolor;
        fgcolor = glyph->bgcolor;
    }
    else
    {
        fgcolor = glyph->fgcolor;
        bgcolor = glyph->bgcolor;
    }

    uint8_t* font_glyph = shift_as(uint8_t*, font->glyphs, c * font->bytes_per_glyph);
    uint32_t offset = data->font_height_offset * y + data->font_width_offset * x;

    for (size_t y = 0; y < font->height; y++, font_glyph += data->font_bytes_per_line, offset += data->pitch)
    {
        line = offset;
        mask = def_mask;
        uint32_t g = *((uint32_t*)font_glyph);

        for (size_t x = 0; x < data->width; x++)
        {
            uint32_t value = g & mask ? fgcolor : bgcolor;
            uint32_t* pixel = (uint32_t*)(data->fb + line);
            *pixel = value;
            mask >>= 1;
            line += 4;
        }
    }
}

static void fbcon_sgr_rgb(console_driver_t*, uint32_t value, uint32_t* color)
{
    *color = value;
}

static void fbcon_sgr_256(console_driver_t*, uint32_t value, uint32_t* color)
{
    if (value >= 232)
    {
        uint8_t c = (value - 232) * 10 + 8;
        *color = c | c << 8 | c << 16;
    }
    else if (value < 16)
    {
        *color = palette[value];
    }
    else
    {
        uint32_t rem;
        value -= 16;

        uint8_t r = ((uint32_t)value * 255) / (36 * 5);
        uint8_t g = ((rem = value % 36) * 255) / (6 * 5);
        uint8_t b = ((rem % 6) * 255) / 5;
        *color = b | g << 8 | r << 16;
    }
}

static void fbcon_sgr_16(console_driver_t*, uint8_t value, uint32_t* color)
{
    *color = palette[value];
}

static void fbcon_sgr_8(console_driver_t*, uint8_t value, uint32_t* color)
{
    *color = palette[value];
}

static void fbcon_defcolor(console_driver_t*, uint32_t* fgcolor, uint32_t* bgcolor)
{
    *fgcolor = COLOR_WHITE;
    *bgcolor = COLOR_BLACK;
}

static int fbcon_font_load(console_driver_t* drv, const void* buffer, size_t size, size_t* resx, size_t* resy)
{
    int errno;
    font_t* new_font = NULL;
    font_t* old_font = NULL;
    data_t* data = drv->data;

    if (unlikely(errno = font_load_from_buffer(buffer, size, &new_font)))
    {
        return errno;
    }

    {
        scoped_irq_lock();

        old_font = data->font;

        fbcon_font_set(data, new_font);
        fbcon_size_set(data, resx, resy);
    }

    if (old_font)
    {
        font_unload(old_font);
    }

    return 0;
}
