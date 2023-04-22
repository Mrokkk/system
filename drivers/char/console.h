#pragma once

#include <kernel/fs.h>

int console_write(struct file*, const char* buffer, int size);
void console_putch(char c);
int console_init(void);

typedef struct
{
    size_t x;
    size_t y;
} vector2_t;

typedef struct line
{
    char* line;
    char* pos;
    size_t index;
    struct line* next;
    struct line* prev;
} line_t;

enum palette
{
    COLOR_BLACK     = 0x000000,
    COLOR_RED       = 0xff0000,
    COLOR_GREEN     = 0x00ff00,
    COLOR_YELLOW    = 0xffff00,
    COLOR_BLUE      = 0x0000ff,
    COLOR_MAGENTA   = 0xff00ff,
    COLOR_CYAN      = 0x00ffff,
    COLOR_WHITE     = 0x999999,
};

#define PARAMS_SIZE 16
#define INITIAL_CAPACITY 128

typedef enum
{
    ES_NORMAL,
    ES_ESC,
    ES_SQUARE,
    ES_GETPARAM,
    ES_GOTPARAM,
} state_t;

typedef struct
{
    vector2_t position;
    vector2_t size;
    size_t capacity;

    uint32_t fg_color;
    uint32_t bg_color;

    uint32_t pitch;
    uint32_t font_height_offset;
    uint32_t font_width_offset;

    line_t* lines;
    line_t* current_line;
    line_t* visible_line;
    line_t* orig_visible_line;
    int scrolling;
    size_t current_index;

    size_t params_nr;
    uint32_t params[PARAMS_SIZE];
    state_t state;

    uint8_t* fb;
} console_t;
