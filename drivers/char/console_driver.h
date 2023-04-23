#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct console_driver
{
    void* data;
    int (*init)(struct console_driver* driver, size_t* row, size_t* col);
    void (*putch)(struct console_driver* driver, size_t row, size_t col, unsigned int c);
    void (*setsgr)(struct console_driver* driver, uint32_t params[], size_t count);
} console_driver_t;
