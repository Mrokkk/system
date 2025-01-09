#pragma once

#include <stdint.h>

enum
{
    GLYPH_ATTR_INVERSED     = 1,
    GLYPH_ATTR_UNDERLINED   = 2,
};

struct glyph
{
    uint32_t fgcolor;
    uint32_t bgcolor;
    uint8_t  c;
    uint8_t  attr;
};

typedef struct glyph glyph_t;
