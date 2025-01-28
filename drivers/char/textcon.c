#include <kernel/vga.h>
#include <kernel/init.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>
#include <kernel/string.h>
#include <kernel/sections.h>
#include <kernel/api/ioctl.h>
#include <kernel/page_alloc.h>
#include <kernel/framebuffer.h>

#include "font.h"
#include "glyph.h"
#include "console_driver.h"

static int textcon_probe(framebuffer_t* fb);
static int textcon_init(console_driver_t* driver, console_config_t* config, size_t* resx, size_t* resy);
static void textcon_glyph_draw(console_driver_t* driver, size_t x, size_t y, glyph_t* glyph);
static void textcon_sgr_16(console_driver_t* driver, uint8_t value, uint32_t* color);
static void textcon_sgr_8(console_driver_t* driver, uint8_t value, uint32_t* color);
static void textcon_defcolor(console_driver_t* driver, uint32_t* fgcolor, uint32_t* bgcolor);
static int textcon_font_load(console_driver_t* driver, const void* buffer, size_t size, size_t* resx, size_t* resy);

#define forecolor(x)        (x)
#define backcolor(x)        (((x) << 4) & 0x7f)
#define blink               (1 << 7)

typedef struct
{
    io16* videomem;
} data_t;

static console_driver_ops_t textcon_ops = {
    .name       = "VGA Text Mode Console",
    .probe      = &textcon_probe,
    .init       = &textcon_init,
    .glyph_draw = &textcon_glyph_draw,
    .defcolor   = &textcon_defcolor,
    .sgr_16     = &textcon_sgr_16,
    .sgr_8      = &textcon_sgr_8,
    .font_load  = &textcon_font_load,
};

static inline void textcon_fb_write(io16* video_mem, uint16_t data, uint16_t offset)
{
    video_mem[offset] = data;
}

static inline uint16_t vga_glyph_make(char c, char a)
{
    return ((a << 8) | c);
}

static int textcon_font_load_internal(console_driver_t*, font_t* font)
{
    int errno;

    scoped_irq_lock();

    if (unlikely(errno = vga_font_set(font->width, font->height, font->glyphs, font->bytes_per_glyph, font->glyphs_count)))
    {
        goto failure;
    }

    font_unload(font);

    return 0;

failure:
    font_unload(font);
    return errno;
}

static int textcon_probe(framebuffer_t* fb)
{
    if (fb->type == FB_TYPE_TEXT && fb->vaddr)
    {
        return 0;
    }

    return -ENODEV;
}

int textcon_init(console_driver_t* driver, console_config_t* config, size_t* resx, size_t* resy)
{
    font_t* font;
    data_t* data = alloc(data_t);

    if (!data)
    {
        log_warning("cannot allocate memory for textcon data");
        return -ENOMEM;
    }

    data->videomem = ptr(framebuffer.vaddr);

    if (!font_load_from_file(config->font_path, &font))
    {
        textcon_font_load_internal(driver, font);
    }

    *resx = framebuffer.width;
    *resy = framebuffer.height;

    driver->data = data;

    vga_cursor_disable();

    return 0;
}

static void textcon_glyph_draw(console_driver_t* drv, size_t x, size_t y, glyph_t* glyph)
{
    data_t* data = drv->data;
    uint16_t off = y * framebuffer.width + x;
    uint32_t fgcolor;
    uint32_t bgcolor;

    glyph_colors_get(glyph, &fgcolor, &bgcolor);

    textcon_fb_write(
        data->videomem,
        vga_glyph_make(glyph->c, forecolor(fgcolor) | backcolor(bgcolor)),
        off);
}

static void textcon_color_set(uint8_t value, uint32_t* color)
{
    *color = vga_palette[value];
}

static void textcon_sgr_16(console_driver_t*, uint8_t value, uint32_t* color)
{
    textcon_color_set(value, color);
}

static void textcon_sgr_8(console_driver_t*, uint8_t value, uint32_t* color)
{
    textcon_color_set(value, color);
}

static void textcon_defcolor(console_driver_t*, uint32_t* fgcolor, uint32_t* bgcolor)
{
    *fgcolor = forecolor(VGA_COLOR_BRIGHTBLACK);
    *bgcolor = backcolor(VGA_COLOR_BLACK);
}

static int textcon_font_load(console_driver_t* drv, const void* buffer, size_t size, size_t* resx, size_t* resy)
{
    int errno;
    font_t* font = NULL;

    if (unlikely(errno = font_load_from_buffer(buffer, size, &font)))
    {
        return errno;
    }

    if (unlikely(errno = textcon_font_load_internal(drv, font)))
    {
        return errno;
    }

    *resx = framebuffer.width;
    *resy = framebuffer.height;

    return 0;
}

UNMAP_AFTER_INIT static int textcon_initialize(void)
{
    console_driver_register(&textcon_ops);
    return 0;
}

premodules_initcall(textcon_initialize);
