#include "fbcon.h"

#include <kernel/kernel.h>

#include "font.h"
#include "glyph.h"
#include "framebuffer.h"
#include "console_driver.h"

#define DEBUG_FBCON       0
#define FONTS_DIR         "/usr/share/"
#define DEFAULT_FONT_PATH FONTS_DIR "font.psf"

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
    uint32_t pitch;
    uint32_t font_height_offset;
    uint32_t font_width_offset;
    uint32_t cursor_offset;
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

int fbcon_init(console_driver_t* driver, size_t* resy, size_t* resx)
{
    int errno;
    data_t* data;

    if ((errno = font_load(DEFAULT_FONT_PATH)))
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
    data->font_height_offset = framebuffer.pitch * font.height;
    data->font_width_offset =  (framebuffer.bpp / 8) * font.width;
    data->cursor_offset = 0;
    driver->data = data;

    *resy = framebuffer.height / font.height;
    *resx = framebuffer.width / font.width;

    return 0;
}

void fbcon_glyph_draw(console_driver_t* drv, size_t y, size_t x, glyph_t* glyph)
{
    data_t* data = drv->data;
    uint32_t line, mask;
    uint8_t c = glyph->c;
    uint32_t fgcolor;
    uint32_t bgcolor;

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

    uint8_t* font_glyph = font.glyphs[c].bytes;
    uint32_t offset = data->font_height_offset * y + data->font_width_offset * x;

    for (uint32_t y = 0; y < font.height; y++, ++font_glyph, offset += data->pitch)
    {
        line = offset;
        mask = 1 << 7;
        for (uint32_t x = 0; x < font.width; x++)
        {
            uint32_t value = *font_glyph & mask ? fgcolor : bgcolor;
            uint32_t* pixel = (uint32_t*)(data->fb + line);
            *pixel = value;
            mask >>= 1;
            line += 4;
        }
    }
}

void fbcon_sgr_rgb(console_driver_t*, uint32_t value, uint32_t* color)
{
    *color = value;
}

void fbcon_sgr_256(console_driver_t*, uint32_t value, uint32_t* color)
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

void fbcon_sgr_16(console_driver_t*, uint8_t value, uint32_t* color)
{
    *color = palette[value];
}

void fbcon_sgr_8(console_driver_t*, uint8_t value, uint32_t* color)
{
    *color = palette[value];
}

void fbcon_defcolor(console_driver_t*, uint32_t* fgcolor, uint32_t* bgcolor)
{
    *fgcolor = COLOR_WHITE;
    *bgcolor = COLOR_BLACK;
}

static void fbcon_draw_line(uint32_t* fb, uint32_t len, uint32_t fgcolor)
{
    for (uint32_t x = 0; x < len; x++)
    {
        uint32_t value = fgcolor;
        uint32_t* pixel = fb;
        *pixel = value;
        ++fb;
    }
}

void fbcon_movecsr(console_driver_t* drv, int x, int y)
{
    data_t* data = drv->data;
    uint32_t old_offset = data->cursor_offset;
    uint32_t new_offset = data->font_height_offset * y + data->font_width_offset * x + data->pitch * (font.height - 1);

    fbcon_draw_line((uint32_t*)(data->fb + new_offset), font.width, COLOR_WHITE);

    if (new_offset != old_offset)
    {
        fbcon_draw_line((uint32_t*)(data->fb + old_offset), font.width, COLOR_BLACK);
    }

    data->cursor_offset = new_offset;
}

int fbcon_font_load(console_driver_t*, const char* name)
{
    char buffer[32];
    csnprintf(buffer, buffer + array_size(buffer), FONTS_DIR "%s.psf", name);

    return font_load(buffer);
}
