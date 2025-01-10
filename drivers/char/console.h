#pragma once

#include <stdint.h>
#include <kernel/fs.h>
#include "glyph.h"
#include "console_driver.h"

typedef struct csi csi_t;
typedef struct line line_t;
typedef struct console console_t;

#define PARAMS_SIZE 16

struct line
{
    glyph_t* glyphs;
};

enum esc
{
    ESC_START = 1,
    ESC_CSI   = 2,
    ESC_OSC   = 4,
};

struct csi
{
    char   prefix;
    size_t params_nr;
    int    params[PARAMS_SIZE];
};

struct console
{
    bool     disabled;
    bool     redraw;
    int      rtc_event_id;
    uint16_t x, y;
    uint16_t prev_x, prev_y;
    uint16_t scroll_top, scroll_bottom;
    uint16_t resx, resy;

    console_driver_t* driver;

    page_t*  pages;
    size_t   capacity;
    size_t   max_capacity;
    line_t*  lines;
    line_t*  current_line;
    line_t*  visible_lines;
    line_t*  orig_visible_lines;
    int      tmux_state;
    uint16_t saved_x, saved_y;
    uint32_t current_fgcolor, current_bgcolor;
    uint8_t  current_attr;
    uint32_t default_fgcolor, default_bgcolor;
    short    escape_state;
    csi_t    csi;
    char     command[32];
    char     command_buf[32];
    uint16_t command_it;

    struct process* kconsole;
};

int console_init(void);
