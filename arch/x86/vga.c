#include <kernel/vga.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>
#include <kernel/page_alloc.h>

typedef struct
{
    uint8_t line[32];
} vga_glyph_t;

static vga_glyph_t* vga_font;

uint8_t vga_palette[] = {
    VGA_COLOR_BLACK,
    VGA_COLOR_RED,
    VGA_COLOR_GREEN,
    VGA_COLOR_YELLOW,
    VGA_COLOR_BLUE,
    VGA_COLOR_MAGENTA,
    VGA_COLOR_CYAN,
    VGA_COLOR_WHITE,
    VGA_COLOR_BRIGHTBLACK,
    VGA_COLOR_BRIGHTRED,
    VGA_COLOR_BRIGHTGREEN,
    VGA_COLOR_BRIGHTYELLOW,
    VGA_COLOR_BRIGHTBLUE,
    VGA_COLOR_BRIGHTMAGENTA,
    VGA_COLOR_BRIGHTCYAN,
    VGA_COLOR_BRIGHTWHITE,
};

int vga_font_set(size_t width, size_t height, void* data, size_t bytes_per_glyph, size_t glyphs_count)
{
    if (width != 8 || height > 32)
    {
        return -EINVAL;
    }

    if (!vga_font)
    {
        vga_font = mmio_map(0xa0000, 256 * 32, "vgafont");

        if (unlikely(!vga_font))
        {
            return -ENOMEM;
        }
    }

    vga_gfx_write(VGA_GFX_MODE, 0x00);
    vga_gfx_write(VGA_GFX_MISC, 0x04);
    vga_seq_write(VGA_SEQ_PLANE_WRITE, 0x04);
    vga_seq_write(VGA_SEQ_MEMORY_MODE, 0x06);

    glyphs_count = min(glyphs_count, 256);

    for (size_t i = 0; i < glyphs_count; ++i, data += bytes_per_glyph)
    {
        memcpy(vga_font[i].line, data, bytes_per_glyph);
        memset(vga_font[i].line + bytes_per_glyph, 0, 32 - bytes_per_glyph);
    }

    memset(vga_font[glyphs_count].line, 0, bytes_per_glyph * (256 - glyphs_count));

    vga_seq_write(VGA_SEQ_PLANE_WRITE, 0x03);
    vga_seq_write(VGA_SEQ_MEMORY_MODE, 0x02);
    vga_gfx_write(VGA_GFX_MODE, 0x10);
    vga_gfx_write(VGA_GFX_MISC, 0x0e);

    return 0;
}

void vga_cursor_disable(void)
{
    vga_crt_write(VGA_CRTC_CURSOR_START, VGA_CRTC_CURSOR_DISABLE);
}
