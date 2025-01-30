#include <kernel/vga.h>
#include <kernel/init.h>
#include <kernel/kernel.h>
#include <kernel/sections.h>
#include <kernel/api/ioctl.h>
#include <kernel/framebuffer.h>

#include "font.h"
#include "glyph.h"
#include "console_driver.h"

#define DEBUG_FBCON             0
#define GLYPH_OFFSET_HORIZONTAL 0

static int fbcon_probe(framebuffer_t* fb);
static int fbcon_probe_truecolor(framebuffer_t* fb);
static int fbcon_init(console_driver_t* driver, console_config_t* config, size_t* resx, size_t* resy);
static void fbcon_glyph_draw(console_driver_t* driver, size_t x, size_t y, glyph_t* glyph);
static void fbcon_glyph_draw_var(console_driver_t* drv, size_t x, size_t y, glyph_t* glyph);
static void fbcon_screen_clear(console_driver_t* driver, uint32_t color);
static void fbcon_sgr_rgb(console_driver_t* driver, uint32_t value, uint32_t* color);
static void fbcon_sgr_256(console_driver_t* driver, uint32_t value, uint32_t* color);
static void fbcon_sgr_16(console_driver_t* driver, uint8_t value, uint32_t* color);
static void fbcon_sgr_8(console_driver_t* driver, uint8_t value, uint32_t* color);
static void fbcon_defcolor(console_driver_t* driver, uint32_t* fgcolor, uint32_t* bgcolor);
static int fbcon_font_load(console_driver_t*, const void* buffer, size_t size, size_t* resx, size_t* resy);
static void fbcon_deinit(console_driver_t* driver);

enum default_palette
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

static console_driver_ops_t fbcon_ops_truecolor = {
    .name         = "FB TrueColor Console",
    .probe        = &fbcon_probe_truecolor,
    .init         = &fbcon_init,
    .deinit       = &fbcon_deinit,
    .glyph_draw   = &fbcon_glyph_draw,
    .sgr_16       = &fbcon_sgr_16,
    .sgr_8        = &fbcon_sgr_8,
    .screen_clear = &fbcon_screen_clear,
    .defcolor     = &fbcon_defcolor,
    .font_load    = &fbcon_font_load,
    .sgr_rgb      = &fbcon_sgr_rgb,
    .sgr_256      = &fbcon_sgr_256,
};

static console_driver_ops_t fbcon_ops = {
    .name         = "FB Console",
    .probe        = &fbcon_probe,
    .init         = &fbcon_init,
    .deinit       = &fbcon_deinit,
    .glyph_draw   = &fbcon_glyph_draw_var,
    .sgr_16       = &fbcon_sgr_16,
    .sgr_8        = &fbcon_sgr_8,
    .screen_clear = &fbcon_screen_clear,
    .defcolor     = &fbcon_defcolor,
    .font_load    = &fbcon_font_load,
    .sgr_rgb      = &fbcon_sgr_rgb,
    .sgr_256      = &fbcon_sgr_256,
};

static void fbcon_fb_set(data_t* data)
{
    data->fb = framebuffer.vaddr;
    data->pitch = framebuffer.pitch;
}

static void fbcon_font_set(data_t* data, font_t* font)
{
    data->font = font;
    data->width = font->width + GLYPH_OFFSET_HORIZONTAL;
    data->font_height_offset = framebuffer.pitch * font->height;
    data->font_width_offset = (align(framebuffer.bpp, 8) / 8) * data->width;
    data->font_bytes_per_line = font->bytes_per_line;
    data->mask = 1 << (font->bytes_per_line * 8 - 1);
}

static void fbcon_size_set(data_t* data, size_t* resx, size_t* resy)
{
    *resx = framebuffer.width / data->width;
    *resy = framebuffer.height / data->font->height;
}

static int fbcon_probe(framebuffer_t* fb)
{
    if (fb->type == FB_TYPE_PACKED_PIXELS && fb->bpp != 32 && fb->vaddr)
    {
        return 0;
    }

    return -ENODEV;
}

static int fbcon_probe_truecolor(framebuffer_t* fb)
{
    if (fb->type == FB_TYPE_PACKED_PIXELS && fb->bpp == 32 && fb->vaddr)
    {
        return 0;
    }

    return -ENODEV;
}

static int fbcon_init(console_driver_t* driver, console_config_t* config, size_t* resx, size_t* resy)
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

    fbcon_fb_set(data);
    fbcon_font_set(data, font);
    fbcon_size_set(data, resx, resy);

    driver->data = data;

    return 0;
}

static uint32_t fbcon_color_convert(uint32_t orig, uint8_t bpp)
{
    switch (bpp)
    {
        case 16:
        {
            uint8_t b = ((orig      ) & 0xff) >> 3;
            uint8_t g = ((orig >> 8 ) & 0xff) >> 2;
            uint8_t r = ((orig >> 16) & 0xff) >> 3;
            return b | (g << 5) | (r << 11);
        }
        case 15:
        {
            uint8_t b = ((orig      ) & 0xff) >> 3;
            uint8_t g = ((orig >> 8 ) & 0xff) >> 3;
            uint8_t r = ((orig >> 16) & 0xff) >> 3;
            return b | (g << 5) | (r << 10);
        }
    }
    return orig;
}

static inline void fbcon_fb_write(uint32_t value, void* pixel, int bytes)
{
    switch (bytes)
    {
        case 1:
            fb_writeb(value, pixel);
            break;
        case 2:
            fb_writew(value, pixel);
            break;
        case 3:
            fb_writeb(value & 0xff, pixel + 0);
            fb_writeb((value >> 8) & 0xff, pixel + 1);
            fb_writeb((value >> 16) & 0xff, pixel + 2);
            break;
        case 4:
            fb_writel(value, pixel);
            break;
    }
}

static inline void fbcon_glyph_draw_var_loop(console_driver_t* drv, size_t x, size_t y, glyph_t* glyph, int bytes)
{
    uint8_t c = glyph->c;
    data_t* data = drv->data;
    font_t* font = data->font;
    uint32_t fgcolor, bgcolor;
    uint32_t def_mask = data->mask;
    fb_rect_t rect = {
        .x = x * data->width,
        .y = y * font->height,
        .w = data->width,
        .h = font->height,
    };

    glyph_colors_get(glyph, &fgcolor, &bgcolor);

    uint8_t* font_glyph = shift_as(uint8_t*, font->glyphs, c * font->bytes_per_glyph);
    uint32_t offset = data->font_height_offset * y + data->font_width_offset * x;

    for (size_t y = 0; y < font->height; y++, font_glyph += data->font_bytes_per_line, offset += data->pitch)
    {
        uintptr_t line = offset;
        uint32_t mask = def_mask;
        uint32_t g = *((uint32_t*)font_glyph);

        for (size_t x = 0; x < data->width; x++, line += bytes, mask >>= 1)
        {
            fbcon_fb_write(g & mask ? fgcolor : bgcolor, data->fb + line, bytes);
        }
    }

    if (framebuffer.ops->dirty_set)
    {
        framebuffer.ops->dirty_set(&rect);
    }
}

static void fbcon_glyph_draw(console_driver_t* drv, size_t x, size_t y, glyph_t* glyph)
{
    fbcon_glyph_draw_var_loop(drv, x, y, glyph, 4);
}

static void fbcon_glyph_draw_var(console_driver_t* drv, size_t x, size_t y, glyph_t* glyph)
{
    uint8_t bytes = align(framebuffer.bpp, 8) / 8;

    switch (bytes)
    {
        case 1:
            fbcon_glyph_draw_var_loop(drv, x, y, glyph, 1);
            break;
        case 2:
            fbcon_glyph_draw_var_loop(drv, x, y, glyph, 2);
            break;
        case 3:
            fbcon_glyph_draw_var_loop(drv, x, y, glyph, 3);
            break;
    }
}

static void fbcon_screen_clear(console_driver_t*, uint32_t color)
{
    uint8_t bytes = align(framebuffer.bpp, 8) / 8;

    fb_rect_t rect = {
        .x = 0,
        .y = 0,
        .w = framebuffer.width,
        .h = framebuffer.height,
    };

    for (size_t off = 0; off < framebuffer.pitch * framebuffer.height; off += bytes)
    {
        fbcon_fb_write(color, framebuffer.vaddr + off, bytes);
    }

    if (framebuffer.ops->dirty_set)
    {
        framebuffer.ops->dirty_set(&rect);
    }
}

static void fbcon_sgr_rgb(console_driver_t*, uint32_t value, uint32_t* color)
{
    if (unlikely(framebuffer.bpp == 8))
    {
        return;
    }
    *color = fbcon_color_convert(value, framebuffer.bpp);
}

static void fbcon_sgr_256(console_driver_t*, uint32_t value, uint32_t* color)
{
    uint32_t temp;
    if (unlikely(framebuffer.bpp == 8))
    {
        return;
    }
    if (value >= 232)
    {
        uint8_t c = (value - 232) * 10 + 8;
        temp = c | c << 8 | c << 16;
    }
    else if (value < 16)
    {
        temp = palette[value];
    }
    else
    {
        uint32_t rem;
        value -= 16;

        uint8_t r = ((uint32_t)value * 255) / (36 * 5);
        uint8_t g = ((rem = value % 36) * 255) / (6 * 5);
        uint8_t b = ((rem % 6) * 255) / 5;
        temp = b | g << 8 | r << 16;
    }
    *color = fbcon_color_convert(temp, framebuffer.bpp);
}

static void fbcon_sgr_16(console_driver_t*, uint8_t value, uint32_t* color)
{
    if (framebuffer.bpp == 8)
    {
        *color = vga_palette[value];
        return;
    }
    *color = fbcon_color_convert(palette[value], framebuffer.bpp);
}

static void fbcon_sgr_8(console_driver_t*, uint8_t value, uint32_t* color)
{
    if (framebuffer.bpp == 8)
    {
        *color = vga_palette[value];
        return;
    }
    *color = fbcon_color_convert(palette[value], framebuffer.bpp);
}

static void fbcon_defcolor(console_driver_t*, uint32_t* fgcolor, uint32_t* bgcolor)
{
    if (framebuffer.bpp == 8)
    {
        *fgcolor = VGA_COLOR_BRIGHTBLACK;
        *bgcolor = VGA_COLOR_BLACK;
    }
    else
    {
        *fgcolor = fbcon_color_convert(COLOR_WHITE, framebuffer.bpp);
        *bgcolor = fbcon_color_convert(COLOR_BLACK, framebuffer.bpp);
    }
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

static void fbcon_deinit(console_driver_t* drv)
{
    data_t* data = drv->data;

    if (data->font)
    {
        font_unload(data->font);
    }
}

UNMAP_AFTER_INIT static int fbcon_initialize(void)
{
    console_driver_register(&fbcon_ops);
    console_driver_register(&fbcon_ops_truecolor);
    return 0;
}

premodules_initcall(fbcon_initialize);
