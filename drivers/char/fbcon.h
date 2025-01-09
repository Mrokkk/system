#pragma once

#include "console_driver.h"

int fbcon_init(console_driver_t* driver, console_config_t* config, size_t* resy, size_t* resx);
void fbcon_glyph_draw(console_driver_t* driver, size_t y, size_t x, glyph_t* glyph);
void fbcon_sgr_rgb(console_driver_t* driver, uint32_t value, uint32_t* color);
void fbcon_sgr_256(console_driver_t* driver, uint32_t value, uint32_t* color);
void fbcon_sgr_16(console_driver_t* driver, uint8_t value, uint32_t* color);
void fbcon_sgr_8(console_driver_t* driver, uint8_t value, uint32_t* color);
void fbcon_defcolor(console_driver_t* driver, uint32_t* fgcolor, uint32_t* bgcolor);
void fbcon_movecsr(console_driver_t* driver, int x, int y);
int fbcon_font_load(console_driver_t*, const void* buffer, size_t size);
