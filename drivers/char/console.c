#define log_fmt(fmt) "console: " fmt
#include "console.h"

#include <stddef.h>
#include <stdint.h>

#include <arch/rtc.h>

#include <kernel/fs.h>
#include <kernel/page.h>
#include <kernel/ioctl.h>
#include <kernel/device.h>
#include <kernel/kernel.h>

#include "tty.h"
#include "fbcon.h"
#include "egacon.h"
#include "keyboard.h"
#include "tty_driver.h"
#include "framebuffer.h"

#define INITIAL_CAPACITY 128

typedef enum
{
    C_PRINT,
    C_DROP,
} command_t;

static int console_open(tty_t* tty, file_t* file);
static int console_setup(console_driver_t* driver);
static int console_close(tty_t* tty, file_t* file);
static int console_write(tty_t* tty, file_t* file, const char* buffer, size_t size);
static int console_ioctl(tty_t* tty, unsigned long request, void* arg);
static void console_putch(tty_t* tty, uint8_t c);

static command_t control(console_t* console, char c);
static int allocate(console_t* console);

static tty_driver_t tty_driver = {
    .name = "tty",
    .major = MAJOR_CHR_TTY,
    .minor_start = 0,
    .num = 1,
    .driver_data = NULL,
    .open = &console_open,
    .close = &console_close,
    .write = &console_write,
    .ioctl = &console_ioctl,
    .putch = &console_putch,
    .initialized = 0,
};

static inline void redraw_lines(console_t* console)
{
    line_char_t* pos;
    line_t* temp;
    size_t x, y, line_index, prev_len;
    console_driver_t* drv = console->driver;
    line_t* prev;

    for (y = 0, temp = console->visible_line, prev = console->prev_visible_line; y < console->resy; ++y)
    {
        pos = temp->line;
        for (x = 0, line_index = 0; pos < temp->pos; ++line_index, ++pos)
        {
            drv->putch(console->driver, y, x, pos->c, pos->fgcolor, pos->bgcolor);
            ++x;
        }
        prev_len = prev->pos - prev->line;
        for (; x < prev_len; ++line_index, ++x)
        {
            drv->putch(console->driver, y, x, ' ', console->default_fgcolor, console->default_bgcolor);
        }
        temp = temp->next;
        prev = prev->next;
    }
}

static inline void redraw_lines_full(console_t* console)
{
    line_char_t* pos;
    line_t* temp;
    size_t x, y, line_index;
    console_driver_t* drv = console->driver;

    for (y = 0, temp = console->visible_line; y < console->resy; ++y)
    {
        pos = temp->line;
        for (x = 0, line_index = 0; pos < temp->pos; ++line_index, ++pos)
        {
            drv->putch(console->driver, y, x, pos->c, pos->fgcolor, pos->bgcolor);
            ++x;
        }
        for (; x < console->resx; ++line_index, ++x)
        {
            drv->putch(console->driver, y, x, ' ', console->default_fgcolor, console->default_bgcolor);
        }
        temp = temp->next;
    }
}

static void console_refresh(void* data)
{
    console_t* console = data;
    if (console->redraw)
    {
        scoped_irq_lock();
        redraw_lines(console);
        console->redraw = false;
        console->prev_visible_line = NULL;
    }
}

static inline void position_newline(console_t* console)
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
            log_error("bug: not the last line!");
        }
        allocate(console);
    }

    ++console->current_index;
    if (console->y < console->resy - 1)
    {
        ++console->y;
    }
    else
    {
        scoped_irq_lock();
        console->prev_visible_line = console->prev_visible_line ? : console->visible_line;
        console->visible_line = console->visible_line->next;
        console->current_line->pos = console->current_line->line;
        console->redraw = true;
        rtc_schedule(&console_refresh, console);
        (void)previous;
    }
}

static inline void position_next(console_t* console)
{
    if (console->x == console->resx - 1)
    {
        position_newline(console);
    }
    else
    {
        ++console->x;
    }
}

static inline void position_prev(console_t* console)
{
    if (!console->x)
    {
        return;
    }
    --console->current_line->pos;
    --console->x;
}

static inline void backspace(console_t* console)
{
    position_prev(console);
    console->driver->putch(console->driver, console->y, console->x, ' ', console->current_fgcolor, console->current_bgcolor);
}

static inline void colors_set(console_t* console)
{
    console_driver_t* drv = console->driver;
    drv->setsgr(drv, console->params, console->params_nr, &console->current_fgcolor, &console->current_bgcolor);
}

static inline void line_nr_print(console_t* console, size_t nr)
{
    char buf[32];
    size_t pos;
    sprintf(buf, "[%d/%d]", nr, console->current_index);
    pos = console->resx - strlen(buf);
    for (size_t i = 0; i < strlen(buf); ++i, ++pos)
    {
        console->driver->putch(console->driver, 0, pos, buf[i], console->default_fgcolor, console->default_bgcolor);
    }
}

static inline void scroll_up(console_t* console)
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

    redraw_lines_full(console);
    line_nr_print(console, line->index);
}

static inline void scroll_down(console_t* console)
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

    redraw_lines_full(console);
    line_nr_print(console, line->index);
}

static inline void scroll(console_t* console)
{
    switch (console->params[0])
    {
        case 5:
            scroll_up(console);
            break;
        case 6:
            scroll_down(console);
            break;
    }
}

static command_t control(console_t* console, char c)
{
    switch (c)
    {
        case 0:
        case 3:
        case 4:
        case 26:
            return C_DROP;
        case '\b':
            backspace(console);
            return C_DROP;
        case '\e':
            log_debug(DEBUG_CONSOLE, "switch to ESC");
            console->state = ES_ESC;
            return C_DROP;
        case '\n':
            position_newline(console);
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
            }
            return C_DROP;
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
                return C_DROP;
            }
            else if (c >= '0' && c <= '9')
            {
                console->params[console->params_nr] *= 10;
                console->params[console->params_nr] += c - '0';
                return C_DROP;
            }
            ++console->params_nr;
            log_debug(DEBUG_CONSOLE, "switch to NORMAL");
            console->state = ES_NORMAL;
            switch (c)
            {
                case 'm':
                    colors_set(console);
                    return C_DROP;
                case '~':
                    scroll(console);
                    return C_DROP;
            }
        case ES_NORMAL:
            break;
    }

    return C_PRINT;
}

static inline void console_putc(console_t* console, uint8_t c)
{
    console->current_line->pos->c = c;
    console->current_line->pos->fgcolor = console->current_fgcolor;
    console->current_line->pos->bgcolor = console->current_bgcolor;
    ++console->current_line->pos;

    if (!console->redraw)
    {
        console->driver->putch(console->driver, console->y, console->x, c, console->current_fgcolor, console->current_bgcolor);
    }

    position_next(console);
}

static void console_putch(tty_t* tty, uint8_t c)
{
    console_t* console = tty->driver->driver_data;
    if (console->disabled)
    {
        return;
    }
    switch (control(console, c))
    {
        case C_PRINT:
            if (console->scrolling)
            {
                console->scrolling = 0;
                console->visible_line = console->orig_visible_line;
                console->orig_visible_line = NULL;
                redraw_lines_full(console);
            }
            if (c == '\t')
            {
                console_putc(console, ' ');
                console_putc(console, ' ');
                console_putc(console, ' ');
                console_putc(console, ' ');
            }
            else
            {
                console_putc(console, c);
            }
            break;
        default:
    }
}

static int console_open(tty_t* tty, file_t*)
{
    int errno;
    console_driver_t* driver;

    if (unlikely(!(driver = alloc(console_driver_t))))
    {
        return -ENOMEM;
    }

    switch (framebuffer.type)
    {
        case FB_TYPE_RGB:
            driver->init = &fbcon_init;
            driver->putch = &fbcon_char_print;
            driver->setsgr = &fbcon_setsgr;
            driver->defcolor = &fbcon_defcolor;
            break;
        case FB_TYPE_TEXT:
            driver->init = &egacon_init;
            driver->putch = &egacon_char_print;
            driver->setsgr = &egacon_setsgr;
            driver->defcolor = &egacon_defcolor;
            break;
        default:
            log_error("cannot initialize console");
            errno = -EINVAL;
            goto error;
    }

    keyboard_init(tty);

    if ((errno = console_setup(driver)))
    {
        log_error("cannot initialize tty video driver");
        goto error;
    }

    return errno;

error:
    delete(driver);
    return errno;
}

static int console_setup(console_driver_t* driver)
{
    int errno;
    line_t* lines;
    line_char_t* strings;
    page_t* pages;
    size_t i, row, col;
    console_t* console;

    if ((errno = driver->init(driver, &row, &col)))
    {
        log_error("driver initialization failed with %d", errno);
        return errno;
    }

    size_t size = sizeof(console_t) + sizeof(line_char_t) * col * INITIAL_CAPACITY + INITIAL_CAPACITY * sizeof(line_t);
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
        lines[i].line = strings + i * col;
        lines[i].pos = lines[i].line;
        lines[i].next = lines + i + 1;
        lines[i].prev = lines + i - 1;
    }

    lines[0].prev = lines + i - 1;
    lines[i - 1].next = lines;

    console->disabled = false;
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
    driver->defcolor(driver, &console->current_fgcolor, &console->current_bgcolor);
    console->default_fgcolor = console->current_fgcolor;
    console->default_bgcolor = console->current_bgcolor;
    console->redraw = false;
    console->prev_visible_line = NULL;

    tty_driver.driver_data = console;

    return 0;
}

static int console_close(tty_t*, file_t*)
{
    return 0;
}

static int console_write(tty_t* tty, file_t*, const char* buffer, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        console_putch(tty, buffer[i]);
    }
    return size;
}

static int console_ioctl(tty_t* tty, unsigned long request, void* arg)
{
    console_t* console = tty->driver->driver_data;
    switch (request)
    {
        case KDSETMODE:
        {
            int mode = (int)arg;
            switch (mode)
            {
                case KD_TEXT:
                    console->disabled = false;
                    redraw_lines_full(console);
                    return 0;
                case KD_GRAPHICS:
                    console->disabled = true;
                    console_refresh(console);
                    return 0;
                default: return -EINVAL;
            }
        }
    }
    return -EINVAL;
}

static int allocate(console_t* console)
{
    size_t i;
    line_char_t* strings;
    line_t* lines;
    page_t* pages;
    size_t line_len = console->resx;
    size_t new_lines = console->capacity * 2;
    size_t size = sizeof(line_char_t) * line_len * new_lines + new_lines * sizeof(line_t);
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
        lines[i].line = lines[i].pos = strings + i * line_len;
        lines[i].next = lines + i + 1;
        lines[i].prev = lines + i - 1;
    }

    console->current_line->next = lines;
    lines[0].prev = console->current_line;
    lines[i - 1].next = console->lines;

    console->capacity += new_lines;
    return 0;
}

UNMAP_AFTER_INIT int console_init(void)
{
    return tty_driver_register(&tty_driver);
}
