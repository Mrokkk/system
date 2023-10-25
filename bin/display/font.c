#include "font.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include "utils.h"
#include "definitions.h"

font_t font;

int font_load(void)
{
    uint32_t* data = file_map(FONT_PATH);

    if (!data)
    {
        return EXIT_FAILURE;
    }

    if (*data == PSF2_FONT_MAGIC)
    {
        psf2_t* psf = (psf2_t*)data;

        font.height = psf->height;
        font.width = psf->width;
        font.bytes_per_glyph = psf->bytes_per_glyph;
        font.glyphs_count = psf->glyph_count;
        font.glyphs = ptr(addr(psf) + psf->header_size);

        return 0;
    }
    else if ((*data & 0xffff) == PSF1_FONT_MAGIC)
    {
        psf1_t* psf = (psf1_t*)data;

        font.height = psf->size;
        font.width = 8;
        font.bytes_per_glyph = psf->size;
        font.glyphs_count = psf->mode & 0x1 ? 512 : 256;
        font.glyphs = ptr(addr(psf) + sizeof(psf1_t));

        return 0;
    }

    return EXIT_FAILURE;
}

void char_to_framebuffer(int x, int y, int c, uint32_t fgcolor, uint32_t bgcolor, uint8_t* framebuffer)
{
    uint32_t line, mask;
    uint8_t* glyph = font.glyphs[c].bytes;
    uint32_t offset = vinfo.pitch * y + x * 4;

    for (uint32_t y = 0; y < font.height; y++, ++glyph, offset += vinfo.pitch)
    {
        line = offset;
        mask = 1 << 7;
        for (uint32_t x = 0; x < font.width; x++)
        {
            uint32_t value = *glyph & mask ? fgcolor : bgcolor;
            uint32_t* pixel = (uint32_t*)(framebuffer + line);
            *pixel = value;
            mask >>= 1;
            line += 4;
        }
    }
}

void string_to_framebuffer(int x, int y, char* string, uint32_t fgcolor, uint32_t bgcolor, uint8_t* framebuffer)
{
    int offset = 0;
    while (*string)
    {
        char_to_framebuffer(x + offset, y, *string, fgcolor, bgcolor, framebuffer);
        offset += font.width;
        ++string;
    }
}
