#pragma once

#include <stdint.h>

#define PSF1_FONT_MAGIC 0x0436
#define PSF2_FONT_MAGIC 0x864ab572

typedef struct
{
    uint8_t bytes[16];
} glyph_t;

typedef struct
{
    uint32_t height;
    uint32_t width;
    uint32_t bytes_per_glyph;
    uint32_t glyphs_count;
    glyph_t* glyphs;
} font_t;

typedef struct
{
    uint16_t magic;
    uint8_t mode;
    uint8_t size;
} __attribute__((packed)) psf1_t;

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t flags;
    uint32_t glyph_count;
    uint32_t bytes_per_glyph;
    uint32_t height;
    uint32_t width;
} __attribute__((packed)) psf2_t;

extern font_t font;

int font_load(void);
void char_to_framebuffer(int x, int y, int c, uint32_t fgcolor, uint32_t bgcolor, uint8_t* framebuffer);
void string_to_framebuffer(int x, int y, char* string, uint32_t fgcolor, uint32_t bgcolor, uint8_t* framebuffer);
