#pragma once

#include "console_driver.h"

int fbcon_init(console_driver_t* driver, size_t* resy, size_t* resx);
void fbcon_char_print(console_driver_t* driver, size_t y, size_t x, unsigned int c);
void fbcon_setsgr(console_driver_t* driver, uint32_t params[], size_t count);
