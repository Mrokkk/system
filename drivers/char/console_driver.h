#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct console_driver console_driver_t;

struct console_driver
{
    void* data;
    int (*init)(console_driver_t* driver, size_t* row, size_t* col);
    void (*putch)(console_driver_t* driver, size_t row, size_t col, uint8_t c, uint32_t fgcolor, uint32_t bgcolor);
    void (*defcolor)(console_driver_t* driver, uint32_t* fgcolor, uint32_t* bgcolor);
    void (*setsgr)(console_driver_t* driver, uint32_t params[], size_t count, uint32_t* fgcolor, uint32_t* bgcolor);
    void (*movecsr)(console_driver_t* driver, int x, int y);
};
