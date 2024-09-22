#define log_fmt(fmt) "console: " fmt
#include "console.h"

#include <stddef.h>
#include <stdint.h>

#include <arch/rtc.h>

#include <kernel/fs.h>
#include <kernel/tty.h>
#include <kernel/page.h>
#include <kernel/ctype.h>
#include <kernel/kernel.h>
#include <kernel/api/ioctl.h>

#include "fbcon.h"
#include "egacon.h"
#include "keyboard.h"
#include "framebuffer.h"

#define INITIAL_CAPACITY 128

static int console_open(tty_t* tty, file_t* file);
static int console_setup(console_driver_t* driver);
static int console_close(tty_t* tty, file_t* file);
static int console_write(tty_t* tty, const char* buffer, size_t size);
static int console_ioctl(tty_t* tty, unsigned long request, void* arg);
static void console_putch_internal(tty_t* tty, int c, int* movecsr);
static void console_putch(tty_t* tty, int c);

typedef enum
{
    C_CONTINUE = 0,
    C_PRINT    = 1,
    C_MOVECSR  = 2,
    C_DROP     = 3,
} command_t;

static command_t control(console_t* console, tty_t* tty, int c);
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

static inline void cursor_update(console_t* console)
{
    console->driver->movecsr(console->driver, console->x, console->y);
}

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
    size_t prev = 0;

    for (y = 0, temp = console->visible_line; y < console->resy; ++y, temp = temp->next)
    {
        if (prev > temp->index)
        {
            for (x = 0; x < console->resx; ++line_index, ++x)
            {
                drv->putch(console->driver, y, x, ' ', console->default_fgcolor, console->default_bgcolor);
            }
            continue;
        }
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
        prev = temp->index;
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
        console->driver->movecsr(console->driver, console->x, console->y);
    }
}

static inline void position_newline(console_t* console)
{
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

static inline void console_write_char(console_t* console, uint8_t c)
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

static inline void carriage_return(console_t* console)
{
    console->current_line->pos = console->current_line->line;
    console->x = 0;
}

static inline void tab(console_t* console)
{
    console_write_char(console, ' ');
    console_write_char(console, ' ');
    console_write_char(console, ' ');
    console_write_char(console, ' ');
    console->driver->movecsr(console->driver, console->x, console->y);
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
    snprintf(buf, sizeof(buf), "[%zu/%zu]", nr, console->current_index);
    pos = console->resx - strlen(buf);
    for (size_t i = 0; i < strlen(buf); ++i, ++pos)
    {
        console->driver->putch(console->driver, 0, pos, buf[i], console->default_fgcolor, console->default_bgcolor);
    }
}

static inline void scroll_up(console_t* console, size_t count)
{
    line_t* line;

    log_debug(DEBUG_CON_SCROLL, "scrolling up by %d lines", count);

    for (line = console->visible_line; count && line != console->lines; line = line->prev, --count);

    if (count == console->resy)
    {
        return;
    }

    console->y -= count;
    if (console->y > console->resy) console->y = 0;

    console->scrolling = 1;
    console->visible_line = line;

    redraw_lines_full(console);
    line_nr_print(console, line->index);
}

static inline void scroll_down(console_t* console, size_t count)
{
    line_t* line;

    log_debug(DEBUG_CON_SCROLL, "scrolling down by %d lines", count);

    for (line = console->visible_line; count && line != console->current_line; line = line->next, --count);

    console->visible_line = line;
    console->y += count;
    if (console->y >= console->resy) console->y = console->resy - 1;

    redraw_lines_full(console);
    line_nr_print(console, line->index);
}

static inline void scroll_to(console_t* console, line_t* line)
{
    console->visible_line = line;

    redraw_lines_full(console);
    line_nr_print(console, line->index);
}

static inline void scroll(console_t* console)
{
    switch (console->params[0])
    {
        case 5:
            scroll_up(console, console->resy);
            break;
        case 6:
            scroll_down(console, console->resy);
            break;
    }
    cursor_update(console);
}

static inline void scroll_line(console_t* console, int dir)
{
    switch (dir)
    {
        case 'A':
            scroll_up(console, 1);
            break;
        case 'B':
            scroll_down(console, 1);
            break;
        case 'C':
            console->x += 1;
            if (console->x >= console->resx) console->x = console->resx - 1;
            break;
        case 'D':
            console->x -= 1;
            if (console->x >= console->resx) console->x = 0;
            break;
    }
    cursor_update(console);
}

static command_t escape_sequence(console_t* console, int c)
{
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
            if (console->tmux_state == '[' &&
                (c == 'A' || c == 'B' || c == 'C' || c == 'D'))
            {
                scroll_line(console, c);
                return C_DROP;
            }
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
                    if (console->tmux_state == '[')
                    {
                        scroll(console);
                    }
                    return C_DROP;
            }
        case ES_NORMAL:
            break;
    }

    return C_CONTINUE;
}

#define TMUX_TRANSITION(from, to, ...) \
    do \
    { \
        if (console->tmux_state == (from)) \
        { \
            if (isprint(from) && isprint(to)) \
            { \
                log_debug(DEBUG_CONSOLE, "tmux_state: from \'%c\' to \'%c\'", from, to); \
            } \
            else if (isprint(from)) \
            { \
                log_debug(DEBUG_CONSOLE, "tmux_state: from \'%u\' to %d", from, to); \
            } \
            else if (isprint(to)) \
            { \
                log_debug(DEBUG_CONSOLE, "tmux_state: from %d to \'%c\'", from, to); \
            } \
            __VA_ARGS__; \
            console->tmux_state = to; \
            return C_DROP; \
        } \
    } \
    while (0)

static command_t tmux_mode(console_t* console, tty_t* tty, int c)
{
    if (likely(!console->tmux_state))
    {
        return C_CONTINUE;
    }

    switch (c)
    {
        case 'g':
            TMUX_TRANSITION('[', 'g', {});
            TMUX_TRANSITION('g', '[',
                {
                    console->y = console->x = 0;
                    scroll_to(console, console->lines);
                });
            break;
        case 'G':
            TMUX_TRANSITION('[', '[',
                {
                    scroll_to(console, console->orig_visible_line);
                    console->y = console->x = 0;
                });
            break;
        case '^':
            console->x = 0;
            cursor_update(console);
            break;
        case '[':
            if (console->state != ES_NORMAL)
            {
                escape_sequence(console, c);
                break;
            }
            TMUX_TRANSITION(TTY_SPECIAL_MODE, '[',
                {
                    console->saved_x = console->x;
                    console->saved_y = console->y;
                    line_nr_print(console, console->current_line->index);
                });
            break;
        case 'q':
            TMUX_TRANSITION('[', 0,
                {
                    tty->special_mode = 0;
                    console->scrolling = 0;
                    console->visible_line = console->orig_visible_line;
                    console->orig_visible_line = NULL;
                    console->x = console->saved_x;
                    console->y = console->saved_y;
                    redraw_lines_full(console);
                    cursor_update(console);
                });
            break;
        case '\e':
            log_debug(DEBUG_CONSOLE, "switch to ESC");
            console->state = ES_ESC;
            break;
        default:
            if (console->state != ES_NORMAL)
            {
                escape_sequence(console, c);
            }
    }
    return C_DROP;
}

static command_t normal_mode(console_t* console, tty_t* tty, int c)
{
    switch (c)
    {
        case 0:
        case 1:
        case 3:
        case 4:
        case 7:
        case 26:
            return C_DROP;
        case '\b':
        case 0x7f:
            position_prev(console);
            return C_MOVECSR;
        case '\e':
            log_debug(DEBUG_CONSOLE, "switch to ESC");
            console->state = ES_ESC;
            return C_DROP;
        case '\t':
            tab(console);
            return C_MOVECSR;
        case '\r':
            carriage_return(console);
            return C_MOVECSR;
        case '\n':
            position_newline(console);
            return C_MOVECSR;
        case TTY_SPECIAL_MODE:
            TMUX_TRANSITION(0, TTY_SPECIAL_MODE,
                {
                    console->tmux_state = TTY_SPECIAL_MODE;
                    console->orig_visible_line = console->visible_line;
                    tty->special_mode = 1;
                });
            return C_DROP;
    }

    return C_CONTINUE;
}

static command_t control(console_t* console, tty_t* tty, int c)
{
    command_t cmd;

    if ((cmd = tmux_mode(console, tty, c)) ||
        (cmd = normal_mode(console, tty, c)) ||
        (cmd = escape_sequence(console, c)))
    {
        return cmd;
    }

    return C_PRINT;
}

static void console_putch_internal(tty_t* tty, int c, int* movecsr)
{
    command_t cmd;
    console_t* console = tty->driver->driver_data;

    if (console->disabled)
    {
        return;
    }

    cmd = control(console, tty, c);

    switch (cmd)
    {
        case C_PRINT:
            console_write_char(console, c);
            fallthrough;
        case C_MOVECSR:
            *movecsr = 1;
            break;
        default:
    }
}

static void console_putch(tty_t* tty, int c)
{
    int movecsr = 0;
    console_putch_internal(tty, c, &movecsr);

    if (movecsr)
    {
        cursor_update(tty->driver->driver_data);
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
            driver->movecsr = &fbcon_movecsr;
            break;
        case FB_TYPE_TEXT:
            driver->init = &egacon_init;
            driver->putch = &egacon_char_print;
            driver->setsgr = &egacon_setsgr;
            driver->defcolor = &egacon_defcolor;
            driver->movecsr = &egacon_movecsr;
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

    tty->driver_special_key = 2; // ctrl+b

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
    console->tmux_state = 0;
    console->saved_x = 0;
    console->saved_y = 0;
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

static int console_write(tty_t* tty, const char* buffer, size_t size)
{
    int movecsr = 0;
    console_t* console = tty->driver->driver_data;
    for (size_t i = 0; i < size; ++i)
    {
        console_putch_internal(tty, buffer[i], &movecsr);
    }

    // Don't move cursor if redraw is scheduled, it will be done there
    if (movecsr && !console->redraw)
    {
        cursor_update(console);
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
            long mode = (long)arg;
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
        case TIOCGWINSZ:
        {
            winsize_t* w = arg;
            w->ws_col = console->resx;
            w->ws_row = console->resy;
            return 0;
        }
        case TIOCSWINSZ:
        {
            winsize_t* w = arg;
            if (w->ws_col == console->resx && w->ws_row == console->resy)
            {
                return 0;
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
