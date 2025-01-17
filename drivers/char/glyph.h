#pragma once

#include <stdint.h>

enum
{
    GLYPH_ATTR_INVERSED     = (1 << 0),
    GLYPH_ATTR_UNDERLINED   = (1 << 1),
};

struct glyph
{
    uint32_t fgcolor;
    uint32_t bgcolor;
    uint8_t  c;
    uint8_t  attr;
};

typedef struct glyph glyph_t;

static inline void glyph_colors_get(glyph_t* glyph, uint32_t* fgcolor, uint32_t* bgcolor)
{
    if (glyph->attr & GLYPH_ATTR_INVERSED)
    {
        *bgcolor = glyph->fgcolor;
        *fgcolor = glyph->bgcolor;
    }
    else
    {
        *fgcolor = glyph->fgcolor;
        *bgcolor = glyph->bgcolor;
    }
}
