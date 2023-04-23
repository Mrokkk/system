#pragma once

#include <stdint.h>
#include <kernel/fs.h>
#include "console_driver.h"

int console_write(struct file*, const char* buffer, int size);
void console_putch(uint8_t c);
int console_init(console_driver_t* driver);

typedef struct line
{
    uint8_t* line;
    uint8_t* pos;
    size_t index;
    struct line* next;
    struct line* prev;
} line_t;

#define PARAMS_SIZE 16

typedef enum
{
    ES_NORMAL,
    ES_ESC,
    ES_SQUARE,
    ES_GETPARAM,
} state_t;

typedef struct
{
    size_t x, y;
    size_t resx, resy;

    console_driver_t* driver;

    size_t capacity;
    line_t* lines;
    line_t* current_line;
    line_t* visible_line;
    line_t* orig_visible_line;
    int scrolling;
    size_t current_index;

    size_t params_nr;
    uint32_t params[PARAMS_SIZE];
    state_t state;
} console_t;
