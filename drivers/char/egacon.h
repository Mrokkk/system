#pragma once

#include "console_driver.h"

int egacon_init(console_driver_t* driver, size_t* row, size_t* col);

void egacon_char_print(
    console_driver_t* driver,
    size_t row,
    size_t col,
    unsigned int c);

void egacon_setsgr(console_driver_t* driver, uint32_t params[], size_t count);
