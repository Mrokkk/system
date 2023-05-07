#pragma once

#include <stddef.h>
#include <stdint.h>

struct console_driver
{
    void* data;
    int (*init)(struct console_driver* driver, size_t* row, size_t* col);
    void (*putch)(struct console_driver* driver, size_t row, size_t col, uint8_t c, uint32_t fgcolor, uint32_t bgcolor);
    void (*defcolor)(struct console_driver* driver, uint32_t* fgcolor, uint32_t* bgcolor);
    void (*setsgr)(struct console_driver* driver, uint32_t params[], size_t count, uint32_t* fgcolor, uint32_t* bgcolor);
};

typedef struct console_driver console_driver_t;
