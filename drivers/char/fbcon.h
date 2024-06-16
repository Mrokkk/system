#pragma once

#include "console_driver.h"

int fbcon_init(console_driver_t* driver, size_t* resy, size_t* resx);
void fbcon_char_print(console_driver_t* driver, size_t y, size_t x, uint8_t c, uint32_t fgcolor, uint32_t bgcolor);
void fbcon_setsgr(console_driver_t* driver, uint32_t params[], size_t count, uint32_t* fgcolor, uint32_t* bgcolor);
void fbcon_defcolor(console_driver_t* driver, uint32_t* fgcolor, uint32_t* bgcolor);
void fbcon_movecsr(console_driver_t* driver, int x, int y);
