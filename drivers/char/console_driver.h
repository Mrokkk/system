#pragma once

#include <stddef.h>
#include <stdint.h>

#include "glyph.h"
#include "console_config.h"

typedef struct console_driver console_driver_t;

struct console_driver
{
    void* data;
    int (*init)(console_driver_t* driver, console_config_t* config, size_t* resx, size_t* resy);
    void (*deinit)(console_driver_t* driver);
    void (*glyph_draw)(console_driver_t* driver, size_t x, size_t y, glyph_t* glyph);
    void (*screen_clear)(console_driver_t* driver, uint32_t color);
    void (*defcolor)(console_driver_t* driver, uint32_t* fgcolor, uint32_t* bgcolor);
    void (*sgr_rgb)(console_driver_t* driver, uint32_t value, uint32_t* color);
    void (*sgr_256)(console_driver_t* driver, uint32_t value, uint32_t* color);
    void (*sgr_16)(console_driver_t* driver, uint8_t value, uint32_t* color);
    void (*sgr_8)(console_driver_t* driver, uint8_t value, uint32_t* color);
    int (*font_load)(console_driver_t* driver, const void* buffer, size_t size, size_t* resx, size_t* resy);
};
