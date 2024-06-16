#pragma once

#include <stdint.h>
#include <kernel/fs.h>
#include "console_driver.h"

typedef struct line line_t;
typedef struct console console_t;
typedef struct line_char line_char_t;

struct line_char
{
    uint32_t fgcolor;
    uint32_t bgcolor;
    uint8_t c;
} PACKED;

struct line
{
    line_char_t* line;
    line_char_t* pos;
    size_t index;
    struct line* next;
    struct line* prev;
};

#define PARAMS_SIZE 16

typedef enum
{
    ES_NORMAL,
    ES_ESC,
    ES_SQUARE,
    ES_GETPARAM,
} state_t;

struct console
{
    bool disabled;
    size_t x, y;
    size_t resx, resy;

    console_driver_t* driver;

    size_t capacity;
    line_t* lines;
    line_t* current_line;
    line_t* visible_line;
    line_t* orig_visible_line;
    int scrolling;
    int tmux_state;
    size_t current_index;
    uint32_t current_fgcolor, current_bgcolor;
    uint32_t default_fgcolor, default_bgcolor;
    bool redraw;
    line_t* prev_visible_line;

    size_t params_nr;
    uint32_t params[PARAMS_SIZE];
    state_t state;
};

int console_init(void);
