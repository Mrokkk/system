#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel/page_alloc.h>

struct font
{
    uint32_t  height;
    uint32_t  width;
    size_t    bytes_per_glyph;
    size_t    bytes_per_line;
    uint32_t  glyphs_count;
    uint8_t*  glyphs;
    page_t*   pages;
};

typedef struct font font_t;

int font_load_from_file(const char* path, font_t** font);
int font_load_from_buffer(const void* buffer, size_t size, font_t** font);
void font_unload(font_t* font);
