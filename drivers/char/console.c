#include "console.h"

#include <stddef.h>
#include <stdint.h>

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

static inline void redraw_lines(size_t prev_line_len)
{
    uint8_t* pos;
    line_t* temp;
    size_t x, y, line_index, temp_len;

    for (y = 0, temp = console->visible_line; y < console->resy; ++y)
    {
        pos = temp->line;
        for (x = 0, line_index = 0; pos < temp->pos; ++line_index, ++pos)
        {
            if (!control(*pos))
            {
                console->driver->putch(console->driver, y, x, *pos);
                ++x;
            }
        }
        temp_len = x;
        for (; x < prev_line_len; ++line_index, ++x)
        {
            console->driver->putch(console->driver, y, x, ' ');
        }
        prev_line_len = temp_len;
        temp = temp->next;
    }
}

static inline void redraw_lines_full()
{
    uint8_t* pos;
    line_t* temp;
    size_t x, y, line_index;

    for (y = 0, temp = console->visible_line; y < console->resy; ++y)
    {
        pos = temp->line;
        for (x = 0, line_index = 0; pos < temp->pos; ++line_index, ++pos)
        {
            if (!control(*pos))
            {
                console->driver->putch(console->driver, y, x, *pos);
                ++x;
            }
        }
        for (; x < console->resx; ++line_index, ++x)
        {
            console->driver->putch(console->driver, y, x, ' ');
        }
        temp = temp->next;
    }
}

static inline void position_newline()
{
    if (console->scrolling)
    {
        return;
    }

    size_t previous = console->visible_line->pos - console->visible_line->line;
    console->current_line = console->current_line->next;
    console->current_line->pos = console->current_line->line;
    console->x = 0;

    if (console->current_index == console->capacity - 2)
    {
        if (unlikely(console->current_line->next != console->lines))
        {
            log_error("bug in console: not the last line!");
        }
        allocate();
    }

    ++console->current_index;
    if (console->y < console->resy - 1)
    {
        ++console->y;
    }
    else if (console->y == console->resy - 1)
    {
        console->visible_line = console->visible_line->next;
        console->current_line->pos = console->current_line->line;
        redraw_lines(previous);
    }
}

static inline void position_next()
{
    if (console->x >= console->resx)
    {
        position_newline();
    }
    else
    {
        ++console->x;
    }
}

static inline void position_prev()
{
    if (!console->x)
    {
        return;
    }
    --console->current_line->pos;
    --console->x;
}

static inline void backspace()
{
    position_prev();
    console->driver->putch(console->driver, console->y, console->x, ' ');
}

static inline void colors_set()
{
    console->driver->setsgr(console->driver, console->params, console->params_nr);
}

static inline void line_nr_print(size_t nr)
{
    char buf[32];
    size_t pos;
    sprintf(buf, "[%d/%d]", nr, console->current_index);
    pos = console->resx - strlen(buf);
    for (size_t i = 0; i < strlen(buf); ++i, ++pos)
    {
        console->driver->putch(console->driver, 0, pos, buf[i]);
    }
}

static inline void scroll_up()
{
    line_t* line;
    size_t count = console->resy;

    log_debug(DEBUG_CON_SCROLL, "scrolling up by %d lines", count);

    for (line = console->visible_line; count && line != console->lines; line = line->prev, --count);

    if (count == console->resy)
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
    size_t count = console->resy;

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
        case 0:
        case 3:
        case 4:
            return C_DROP;
        case '\b':
            backspace();
            return C_DROP;
        case '\e':
            log_debug(DEBUG_CONSOLE, "switch to ESC");
            console->state = ES_ESC;
            return C_SAVE;
        case '\n':
            position_newline();
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
            ++console->params_nr;
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

void console_putch(uint8_t c)
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
            console->driver->putch(console->driver, console->y, console->x, c);
            position_next();
            break;
        case C_SAVE:
            *console->current_line->pos++ = c;
            break;
        case C_DROP:
    }
}

int console_write(struct file*, const char* buffer, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        console_putch(buffer[i]);
    }
    return size;
}

static int allocate()
{
    size_t i;
    uint8_t* strings;
    line_t* lines;
    page_t* pages;
    size_t line_len = console->resx * 2;
    size_t new_lines = console->capacity * 2;
    size_t size = line_len * new_lines + new_lines * sizeof(line_t);
    size_t needed_pages = page_align(size) / PAGE_SIZE;

    log_debug(DEBUG_CONSOLE, "allocating %u B (%u pages) for %u more lines", size, needed_pages, new_lines);

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
        lines[i].index = i + console->capacity;
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

#define INITIAL_CAPACITY 128

int console_init(console_driver_t* driver)
{
    int errno;
    line_t* lines;
    uint8_t* strings;
    page_t* pages;
    size_t i, row, col;

    if ((errno = driver->init(driver, &row, &col)))
    {
        log_error("driver initialization failed with %d", errno);
        return errno;
    }

    size_t line_len = col * 2;
    size_t size = sizeof(console_t) + line_len * INITIAL_CAPACITY + INITIAL_CAPACITY * sizeof(line_t);
    size_t needed_pages = page_align(size) / PAGE_SIZE;

    log_notice("size: %u x %u; need %u B (%u pages) for %u lines", col, row, size, needed_pages, INITIAL_CAPACITY);

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

    console->x = 0;
    console->y = 0;
    console->resx = col;
    console->resy = row;
    console->lines = lines;
    console->current_line = lines;
    console->visible_line = lines;
    console->orig_visible_line = NULL;
    console->scrolling = 0;
    console->current_index = 0;
    console->capacity = INITIAL_CAPACITY;
    console->driver = driver;

    return 0;
}
