#pragma once

#include "console_driver.h"

int egacon_init(console_driver_t* driver, size_t* row, size_t* col);

void egacon_glyph_draw(
    console_driver_t* driver,
    size_t row,
    size_t col,
    glyph_t* glyph);

void egacon_sgr_16(console_driver_t* driver, uint8_t value, uint32_t* color);
void egacon_sgr_8(console_driver_t* driver, uint8_t value, uint32_t* color);
void egacon_setsgr(console_driver_t* driver, int params[], size_t count, uint32_t* fgcolor, uint32_t* bgcolor);
void egacon_defcolor(console_driver_t* driver, uint32_t* fgcolor, uint32_t* bgcolor);
void egacon_movecsr(console_driver_t* driver, int x, int y);
