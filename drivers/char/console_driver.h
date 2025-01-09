#pragma once

#include <stddef.h>
#include <stdint.h>

#include "glyph.h"

typedef struct console_driver console_driver_t;

struct console_driver
{
    void* data;
    int (*init)(console_driver_t* driver, size_t* row, size_t* col);
    void (*glyph_draw)(console_driver_t* driver, size_t row, size_t col, glyph_t* glyph);
    void (*defcolor)(console_driver_t* driver, uint32_t* fgcolor, uint32_t* bgcolor);
    void (*sgr_rgb)(console_driver_t* driver, uint32_t value, uint32_t* color);
    void (*sgr_256)(console_driver_t* driver, uint32_t value, uint32_t* color);
    void (*sgr_16)(console_driver_t* driver, uint8_t value, uint32_t* color);
    void (*sgr_8)(console_driver_t* driver, uint8_t value, uint32_t* color);
    void (*movecsr)(console_driver_t* driver, int x, int y);
    int (*font_load)(console_driver_t* driver, const char* name);
};
