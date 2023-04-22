#include "console.h"

#include <stddef.h>
#include <stdint.h>

#include <kernel/font.h>
#include <kernel/page.h>
#include <kernel/kernel.h>

#include "framebuffer.h"

console_t* console;

typedef enum
{
    C_PRINT,
    C_SAVE,
    C_DROP,
} command_t;

static command_t control(char c);
static int allocate();

static inline void char_print(
    size_t row,
    size_t col,
    int c,
    uint32_t fg_color,
    uint32_t bg_color)
{
    uint32_t line, mask;
    uint8_t* glyph = font.glyphs[c].bytes;
    uint32_t offset = console->font_height_offset * row + console->font_width_offset * col;

    for (uint32_t y = 0; y < font.bytes_per_glyph; y++, ++glyph, offset += console->pitch)
    {
        line = offset;
        mask = 1 << 7;
        for (uint32_t x = 0; x < font.bytes_per_glyph; x++)
        {
            uint32_t value = *glyph & mask ? fg_color : bg_color;
            uint32_t* pixel = (uint32_t*)(console->fb + line);
            *pixel = value;
            mask >>= 1;
            line += 4;
        }
    }
}

static inline void redraw_lines(size_t prev_line_len)
{
    char* pos;
    line_t* temp;
    size_t x, y, line_index, temp_len;

    for (y = 0, temp = console->visible_line; y < console->size.y; ++y)
    {
        pos = temp->line;
        for (x = 0, line_index = 0; pos < temp->pos; ++line_index, ++pos)
        {
            if (!control(*pos))
            {
                char_print(y, x, *pos, console->fg_color, console->bg_color);
                ++x;
            }
        }
        temp_len = x;
        for (; x < prev_line_len; ++line_index, ++x)
        {
            char_print(y, x, ' ', console->fg_color, console->bg_color);
        }
        prev_line_len = temp_len;
        temp = temp->next;
    }
}

static inline void redraw_lines_full()
{
    char* pos;
    line_t* temp;
    size_t x, y, line_index;

    for (y = 0, temp = console->visible_line; y < console->size.y; ++y)
    {
        pos = temp->line;
        for (x = 0, line_index = 0; pos < temp->pos; ++line_index, ++pos)
        {
            if (!control(*pos))
            {
                char_print(y, x, *pos, console->fg_color, console->bg_color);
                ++x;
            }
        }
        for (; x < console->size.x - 1; ++line_index, ++x)
        {
            char_print(y, x, ' ', console->fg_color, console->bg_color);
        }
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

    if (console->current_index == console->capacity - 2)
    {
        if (unlikely(console->current_line->next != console->lines))
        {
            log_error("bug in console: not the last line!");
        }
        allocate();
    }

    ++console->current_index;
    if (console->position.y < console->size.y - 1)
    {
        ++console->position.y;
    }
    else if (console->position.y == console->size.y - 1)
    {
        console->visible_line = console->visible_line->next;
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

static inline void backspace()
{
    position_prev();
    char_print(console->position.y, console->position.x, ' ', console->fg_color, console->bg_color);
}

typedef enum
{
    NORMAL,
    COLOR256,
    RGB,
} color_mode_t;

static inline void colors_set()
{
    color_mode_t mode = NORMAL;
    uint32_t color = 0;
    for (size_t i = 0; i < console->params_nr; ++i)
    {
        uint32_t param = console->params[i];
        log_debug(DEBUG_CONSOLE, "param %d = %d", i, param);

        if (mode == RGB)
        {
            log_debug(DEBUG_CONSOLE, "color = %d", param);
            color = (color << 8) | param;
            continue;
        }

        switch (param)
        {
            case 0:
                console->fg_color = COLOR_WHITE;
                console->bg_color = COLOR_BLACK;
                break;
            case 30: console->fg_color = COLOR_BLACK; break;
            case 31: console->fg_color = COLOR_RED; break;
            case 32: console->fg_color = COLOR_GREEN; break;
            case 33: console->fg_color = COLOR_YELLOW; break;
            case 34: console->fg_color = COLOR_BLUE; break;
            case 35: console->fg_color = COLOR_MAGENTA; break;
            case 36: console->fg_color = COLOR_CYAN; break;
            case 38:
                if (console->params[++i] == 2)
                {
                    mode = RGB;
                }
                break;
            case 40: console->bg_color = COLOR_BLACK; break;
            case 41: console->bg_color = COLOR_RED; break;
            case 42: console->bg_color = COLOR_GREEN; break;
            case 43: console->bg_color = COLOR_YELLOW; break;
            case 44: console->bg_color = COLOR_BLUE; break;
            case 45: console->bg_color = COLOR_MAGENTA; break;
            case 46: console->bg_color = COLOR_CYAN; break;
        }
    }
    if (color)
    {
        console->fg_color = color;
    }
}

static inline void line_nr_print(size_t nr)
{
    char buf[32];
    size_t pos;
    sprintf(buf, "[%d/%d]", nr, console->current_index);
    pos = console->size.x - 1 - strlen(buf);
    for (size_t i = 0; i < strlen(buf); ++i, ++pos)
    {
        char_print(0, pos, buf[i], COLOR_BLACK, 0xeab676);
    }
}

static inline void scroll_up()
{
    line_t* line;
    size_t count = console->size.y;

    log_debug(DEBUG_CON_SCROLL, "scrolling up by %d lines", count);

    for (line = console->visible_line; count && line != console->lines; line = line->prev, --count);

    if (count == console->size.y)
    {
        return;
    }

    if (!console->scrolling)
    {
        console->orig_visible_line = console->visible_line;
    }

    console->scrolling = 1;
    console->visible_line = line;

    redraw_lines_full();
    line_nr_print(line->index);
}

static inline void scroll_down()
{
    line_t* line;
    size_t count = console->size.y;

    if (!console->scrolling)
    {
        return;
    }

    log_debug(DEBUG_CON_SCROLL, "scrolling down by %d lines", count);

    for (line = console->visible_line; count && line != console->current_line; line = line->next, --count);

    console->visible_line = line;

    redraw_lines_full();
    line_nr_print(line->index);
}

static inline void scroll()
{
    switch (console->params[0])
    {
        case 5:
            scroll_up();
            break;
        case 6:
            scroll_down();
            break;
    }
}

static command_t control(char c)
{
    switch (c)
    {
        case '\0':
            return C_DROP;
        case '\b':
            backspace();
            return C_DROP;
        case '\e':
            log_debug(DEBUG_CONSOLE, "switch to ESC");
            console->state = ES_ESC;
            return C_SAVE;
        case '\n':
            if (!console->scrolling)
            {
                position_newline();
            }
            return C_DROP;
    }
    switch (console->state)
    {
        case ES_ESC:
            console->state = ES_NORMAL;
            switch (c)
            {
                case '[':
                    log_debug(DEBUG_CONSOLE, "switch to [");
                    console->state = ES_SQUARE;
                    return C_SAVE;
            }
            return C_SAVE;
        case ES_SQUARE:
            memset(console->params, 0, sizeof(uint32_t) * PARAMS_SIZE);
            console->params_nr = 0;
            console->state = ES_GETPARAM;
            log_debug(DEBUG_CONSOLE, "switch to GETPARAM");
            fallthrough;
        case ES_GETPARAM:
            if (c == ';' && console->params_nr < PARAMS_SIZE - 1)
            {
                ++console->params_nr;
                return C_SAVE;
            }
            else if (c >= '0' && c <= '9')
            {
                console->params[console->params_nr] *= 10;
                console->params[console->params_nr] += c - '0';
                return C_SAVE;
            }
            else
            {
                log_debug(DEBUG_CONSOLE, "switch to GOTPARAM");
                console->state = ES_GOTPARAM;
                ++console->params_nr;
            }
            fallthrough;
        case ES_GOTPARAM:
            log_debug(DEBUG_CONSOLE, "switch to NORMAL");
            console->state = ES_NORMAL;
            switch (c)
            {
                case 'm':
                    colors_set();
                    return C_SAVE;
                case '~':
                    scroll();
                    return C_DROP;
            }
        case ES_NORMAL:
            break;
    }
    return C_PRINT;
}

void console_putch(char c)
{
    switch (control(c))
    {
        case C_PRINT:
            if (console->scrolling)
            {
                console->scrolling = 0;
                console->visible_line = console->orig_visible_line;
                console->orig_visible_line = NULL;
                redraw_lines_full();
            }
            *console->current_line->pos++ = c;
            char_print(console->position.y, console->position.x, c, console->fg_color, console->bg_color);
            position_next();
            break;
        case C_SAVE:
            *console->current_line->pos++ = c;
            fallthrough;
        case C_DROP:
    }
}

int console_write(struct file*, const char* buffer, int size)
{
    for (int i = 0; i < size; ++i)
    {
        console_putch(buffer[i]);
    }
    return size;
}

static int allocate()
{
    size_t i;
    char* strings;
    line_t* lines;
    page_t* pages;
    size_t line_len = console->size.x * 2;
    size_t new_lines = console->capacity * 2;
    size_t size = line_len * new_lines + new_lines * sizeof(line_t);
    size_t needed_pages = page_align(size) / PAGE_SIZE;

    log_debug(DEBUG_CONSOLE, "allocating %x B (%u pages) for lines %d", size, needed_pages, INITIAL_CAPACITY);

    pages = page_alloc(needed_pages, PAGE_ALLOC_CONT);

    if (unlikely(!pages))
    {
        log_warning("cannot allocate pages for lines!");
        return -ENOMEM;
    }

    lines = page_virt_ptr(pages);
    strings = ptr(addr(lines) + new_lines * sizeof(line_t));

    for (i = 0; i < new_lines; ++i)
    {
        lines[i].index = i;
        lines[i].line = strings + i * line_len;
        lines[i].pos = lines[i].line;
        lines[i].next = lines + i + 1;
        lines[i].prev = lines + i - 1;
    }

    console->current_line->next = lines;
    lines[0].prev = console->current_line;
    lines[i - 1].next = console->lines;

    console->capacity += new_lines;
    return 0;
}

int console_init()
{
    size_t i;
    line_t* lines;
    char* strings;
    page_t* pages;

    size_t row = framebuffer.height / font.height;
    size_t col = framebuffer.width / font.width;
    size_t line_len = col * 2;
    size_t size = sizeof(console_t) + line_len * INITIAL_CAPACITY + INITIAL_CAPACITY * sizeof(line_t);
    size_t needed_pages = page_align(size) / PAGE_SIZE;

    log_notice("size: %u x %u; need %x B (%u pages) for lines", col, row, size, needed_pages);

    pages = page_alloc(needed_pages, PAGE_ALLOC_CONT);

    if (unlikely(!pages))
    {
        log_warning("cannot allocate pages for lines!");
        return -1;
    }

    console = page_virt_ptr(pages);
    lines = ptr(addr(console) + sizeof(console_t));
    strings = ptr(addr(lines) + INITIAL_CAPACITY * sizeof(line_t));

    for (i = 0; i < INITIAL_CAPACITY; ++i)
    {
        lines[i].index = i;
        lines[i].line = strings + i * line_len;
        lines[i].pos = lines[i].line;
        lines[i].next = lines + i + 1;
        lines[i].prev = lines + i - 1;
    }

    lines[0].prev = lines + i - 1;
    lines[i - 1].next = lines;

    console->position.x = 0;
    console->position.y = 0;
    console->size.x = col;
    console->size.y = row;
    console->lines = lines;
    console->current_line = lines;
    console->visible_line = lines;
    console->orig_visible_line = NULL;
    console->scrolling = 0;
    console->current_index = 0;
    console->fg_color = COLOR_WHITE;
    console->bg_color = COLOR_BLACK;
    console->pitch = framebuffer.pitch;
    console->font_height_offset = framebuffer.pitch * font.height;
    console->font_width_offset =  (framebuffer.bpp / 8) * font.width;
    console->fb = framebuffer.fb;
    console->capacity = INITIAL_CAPACITY;

    return 0;
}
