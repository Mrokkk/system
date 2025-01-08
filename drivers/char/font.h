#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel/page_alloc.h>

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
    page_t*  pages;
} font_t;

extern font_t font;

int font_load(const char* path);
