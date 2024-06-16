#pragma once

#include "console_driver.h"

int egacon_init(console_driver_t* driver, size_t* row, size_t* col);

void egacon_char_print(
    console_driver_t* driver,
    size_t row,
    size_t col,
    uint8_t c,
    uint32_t fgcolor,
    uint32_t bgcolor);

void egacon_setsgr(console_driver_t* driver, uint32_t params[], size_t count, uint32_t* fgcolor, uint32_t* bgcolor);
void egacon_defcolor(console_driver_t* driver, uint32_t* fgcolor, uint32_t* bgcolor);
void egacon_movecsr(console_driver_t* driver, int x, int y);
