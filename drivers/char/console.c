#include "console.h"

#include <stddef.h>
#include <stdint.h>

#include <arch/page.h>

#include <kernel/font.h>
#include <kernel/kernel.h>

#include "framebuffer.h"

typedef struct
{
    size_t x;
    size_t y;
} vector2_t;

typedef struct line
{
    char* line;
    char* pos;
    struct line* next;
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

typedef struct
{
    vector2_t position;
    vector2_t size;

    uint32_t fg_color;
    uint32_t bg_color;

    line_t* lines;
    line_t* current_line;

#define ESCAPE_SEQUENCE_START '\e'
#define ESCAPE_SEQUENCE_CONT '['
#define ESCAPE_SEQUENCE_BG ';'
    int escape_sequence;
    int escape_index;
    char escape_color[3];
    uint8_t* fb;
} console_t;

console_t* console;

static inline void char_print(
    size_t row,
    size_t col,
    int c,
    uint32_t fg_color)
{
    uint32_t pitch = framebuffer.pitch;
    uint32_t bytes_per_pixel = framebuffer.bpp / 8;
    uint8_t* glyph = font.glyphs[c].bytes;
    uint32_t offs = font.height * pitch * row + font.width * bytes_per_pixel * col;
    uint32_t line, mask;

    for (uint32_t y = 0; y < font.bytes_per_glyph; y++)
    {
        line = offs;
        mask = 1 << 7;
        for (uint32_t x = 0; x < font.bytes_per_glyph; x++)
        {
            uint32_t value = *glyph & mask ? fg_color : 0;
            uint32_t* pixel = (uint32_t*)(console->fb + line);
            *pixel = value;
            mask >>= 1;
            line += 4;
        }
        glyph += 1;
        offs += 0x1000;
    }
}

static inline void color_set(uint32_t* color, char* code)
{
    if (code[0] == '3' && code[1] == '0')
    {
        *color = COLOR_BLACK;
    }
    else if (code[0] == '3' && code[1] == '1')
    {
        *color = COLOR_RED;
    }
    else if (code[0] == '3' && code[1] == '2')
    {
        *color = COLOR_GREEN;
    }
    else if (code[0] == '3' && code[1] == '3')
    {
        *color = COLOR_YELLOW;
    }
    else if (code[0] == '3' && code[1] == '4')
    {
        *color = COLOR_BLUE;
    }
    else if (code[0] == '3' && code[1] == '5')
    {
        *color = COLOR_MAGENTA;
    }
    else if (code[0] == '3' && code[1] == '6')
    {
        *color = COLOR_CYAN;
    }
    else if (*code == '0')
    {
        *color = COLOR_WHITE;
    }
}

static inline int escape_sequence_handle(char c)
{
    if (c == ESCAPE_SEQUENCE_START)
    {
        console->escape_sequence = c;
        return c;
    }
    else if (c == ESCAPE_SEQUENCE_CONT && console->escape_sequence == ESCAPE_SEQUENCE_START)
    {
        console->escape_sequence = c;
        return c;
    }
    else if (console->escape_sequence == ESCAPE_SEQUENCE_CONT)
    {
        if (c == ESCAPE_SEQUENCE_BG)
        {
            console->escape_index = 0;
            console->escape_sequence = ESCAPE_SEQUENCE_BG;
            color_set(&console->fg_color, console->escape_color);
        }
        else if (c == 'm')
        {
            console->escape_sequence = 0;
            console->escape_index = 0;
            color_set(&console->fg_color, console->escape_color);
        }
        else if (console->escape_index < 3)
        {
            console->escape_color[console->escape_index++] = c;
        }
        return c;
    }
    else if (console->escape_sequence == ESCAPE_SEQUENCE_BG)
    {
        if (c == 'm')
        {
            color_set(&console->bg_color, console->escape_color);
            console->escape_sequence = 0;
            console->escape_index = 0;
        }
        else if (console->escape_index < 3)
        {
            console->escape_color[console->escape_index++] = c;
        }
        return c;
    }
    return 0;
}

static inline void redraw_lines(size_t prev_line_len)
{
    char* pos;
    line_t* temp;
    size_t x, y, line_index, temp_len;

    for (y = 0, temp = console->lines; y < console->size.y; ++y)
    {
        pos = temp->line;
        for (x = 0, line_index = 0; pos < temp->pos; ++line_index, ++pos)
        {
            if (!escape_sequence_handle(*pos))
            {
                char_print(y, x, *pos, console->fg_color);
                ++x;
            }
        }
        temp_len = x;
        for (; x < prev_line_len; ++line_index, ++x)
        {
            char_print(y, x, ' ', console->fg_color);
        }
        prev_line_len = temp_len;
        temp = temp->next;
    }
}

static inline void position_next()
{
    if (console->position.x >= console->size.x)
    {
        console->position.x = 0;
        ++console->position.y;
    }
    else
    {
        ++console->position.x;
    }
}

static inline void position_newline()
{
    size_t previous = console->lines->pos - console->lines->line;
    console->current_line = console->current_line->next;
    console->current_line->pos = console->current_line->line;
    console->position.x = 0;

    if (console->position.y < console->size.y - 1)
    {
        ++console->position.y;
    }
    else if (console->position.y == console->size.y - 1)
    {
        console->lines = console->lines->next;
        console->current_line->pos = console->current_line->line;
        redraw_lines(previous);
    }
}

static inline void position_prev()
{
    if (!console->position.x)
    {
        return;
    }
    --console->current_line->pos;
    --console->position.x;
}

void console_putch(char c)
{
    flags_t flags;
    irq_save(flags);

    if (c == '\n')
    {
        position_newline();
        goto finish;
    }
    else if (c == '\b')
    {
        position_prev();
        char_print(console->position.y, console->position.x, ' ', console->fg_color);
        goto finish;
    }
    else if (escape_sequence_handle(c))
    {
        *console->current_line->pos++ = c;
        goto finish;
    }

    *console->current_line->pos++ = c;
    char_print(console->position.y, console->position.x, c, console->fg_color);
    position_next();

finish:
    irq_restore(flags);
}

int console_write(struct file*, char* buffer, int size)
{
    for (int i = 0; i < size; ++i)
    {
        console_putch(buffer[i]);
    }
    return size;
}

void lines_setup(line_t* lines, char* strings, size_t line_len, size_t row)
{
    size_t i;
    for (i = 0; i < row; ++i)
    {
        lines[i].line = strings + i * line_len;
        lines[i].next = lines + i + 1;
    }

    lines[i - 1].next = lines;
}

// FIXME: kmalloc has a bug and crashes
int console_init()
{
    size_t i;
    line_t* lines;
    char* strings;
    page_t* pages;

    size_t row = row = framebuffer.height / font.height;
    size_t col = framebuffer.width / font.width;
    size_t line_len = col * 2;
    size_t needed_pages = page_align(sizeof(console_t) + line_len * row + row * sizeof(line_t)) / PAGE_SIZE;

    log_notice("size: %u x %u; need %u pages for lines", col, row, needed_pages);

    pages = page_alloc(needed_pages, PAGE_ALLOC_CONT);

    if (unlikely(!pages))
    {
        log_warning("cannot allocate pages for lines!");
        return -1;
    }

    console = page_virt_ptr(pages);
    lines = ptr(addr(console) + sizeof(console_t));
    strings = ptr(addr(lines) + row * sizeof(line_t));

    for (i = 0; i < row; ++i)
    {
        lines[i].line = strings + i * line_len;
        lines[i].pos = lines[i].line;
        lines[i].next = lines + i + 1;
    }

    lines[i - 1].next = lines;

    console->position.x = 0;
    console->position.y = 0;
    console->size.x = col;
    console->size.y = row;
    console->lines = lines;
    console->current_line = lines;
    console->fg_color = COLOR_WHITE;
    console->bg_color = COLOR_BLACK;
    console->escape_sequence = 0;
    console->escape_index = 0;
    console->fb = framebuffer.fb;

    return 0;
}
