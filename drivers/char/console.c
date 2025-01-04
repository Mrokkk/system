#define log_fmt(fmt) "console: " fmt
#include "console.h"

#include <stddef.h>
#include <stdint.h>

#include <arch/rtc.h>

#include <kernel/fs.h>
#include <kernel/tty.h>
#include <kernel/ctype.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>
#include <kernel/string.h>
#include <kernel/api/ioctl.h>
#include <kernel/page_alloc.h>

#include "fbcon.h"
#include "egacon.h"
#include "keyboard.h"
#include "framebuffer.h"

// References:
// https://invisible-island.net/xterm/ctlseqs/ctlseqs.html

#define DEBUG_CONSOLE         0
#define DEBUG_CON_SCROLL      0
#define CONSOLE_DRIVEN_CURSOR 1
#define INITIAL_CAPACITY      128
#define MAX_CAPACITY          4096

static int console_open(tty_t* tty, file_t* file);
static int console_setup(console_driver_t* driver);
static int console_close(tty_t* tty, file_t* file);
static int console_write(tty_t* tty, const char* buffer, size_t size);
static int console_ioctl(tty_t* tty, unsigned long request, void* arg);
static void console_putch_internal(console_t* console, tty_t* tty, int c, int* movecsr);
static void console_putch(tty_t* tty, int c);
static void scroll_up(console_t* console, size_t origin, size_t count);
static void scroll_down(console_t* console, size_t origin, size_t count);
static int extend(console_t* console);
static int clear_cmd(console_t* console, const char*[]);

static tty_driver_t tty_driver = {
    .name = "tty",
    .major = MAJOR_CHR_TTY,
    .num = 1,
    .open = &console_open,
    .close = &console_close,
    .write = &console_write,
    .ioctl = &console_ioctl,
    .putch = &console_putch,
};

struct command
{
    const char* name;
    int (*fn)(console_t*, const char*[]);
};

typedef struct command command_t;

#define COMMAND(cmd) \
    {.name = #cmd, .fn = &cmd##_cmd}

static READONLY const command_t commands[] = {
    COMMAND(clear),
    {NULL, NULL}
};

#define LINE_INDEX(line) ({ (size_t)((line) - console->lines); })

#define LIMIT(val, min, max) \
    do \
    { \
        if (val < (typeof(val))min) val = min; \
        else if (val > (typeof(val))max) val = max; \
    } \
    while (0)

#define ULIMIT(val, max) \
    do \
    { \
        if (val > (typeof(val))max) val = max; \
    } \
    while (0)

#define DEFAULT_VALUE(param, val) \
    do \
    { \
        if (!(param)) \
        { \
            param = (val); \
        } \
    } \
    while (0)

#define BETWEEN(val, a, b) ((val) >= (a) && (val) <= (b))

#define GLYPH_CLEAR(g) \
    { \
        glyph_t* _glyph = g; \
        _glyph->c = ' '; \
        _glyph->fgcolor = console->default_fgcolor; \
        _glyph->bgcolor = console->default_bgcolor; \
    }

static void cursor_update(console_t* console)
{
#if CONSOLE_DRIVEN_CURSOR
    glyph_t* glyph = &console->visible_lines[console->y].glyphs[console->x];
    console->driver->putch(console->driver, console->y, console->x, glyph->c, glyph->bgcolor, glyph->fgcolor);

    if (console->prev_x != console->x || console->prev_y != console->y)
    {
        glyph = &console->visible_lines[console->prev_y].glyphs[console->prev_x];
        console->driver->putch(console->driver, console->prev_y, console->prev_x, glyph->c, glyph->fgcolor, glyph->bgcolor);

        console->prev_x = console->x;
        console->prev_y = console->y;
    }
#else
    console->driver->movecsr(console->driver, console->x, console->y);
#endif
}

static void current_line_clear(console_t* console, int mode)
{
    console_driver_t* drv = console->driver;
    line_t* line = console->current_line;
    size_t start, end;

    switch (mode)
    {
        case 0:
            start = console->x;
            end = console->resx;
            break;
        case 1:
            start = 0;
            end = console->x;
            break;
        case 2:
            start = 0;
            end = console->resx;
            break;
        default:
            log_info("incorrect clear mode");
            return;
    }

    for (size_t x = start; x < end; ++x)
    {
        glyph_t* glyph = &line->glyphs[x];
        GLYPH_CLEAR(glyph);
        drv->putch(console->driver, console->y, x, ' ', console->default_fgcolor, console->default_bgcolor);
    }
}

static void redraw(console_t* console)
{
    size_t x, y;
    glyph_t* pos;
    line_t* temp;
    console_driver_t* drv = console->driver;

    for (y = 0, temp = console->visible_lines; y < console->resy; ++y, temp++)
    {
        for (pos = temp->glyphs, x = 0; x < console->resx; ++pos, ++x)
        {
            drv->putch(console->driver, y, x, pos->c, pos->fgcolor, pos->bgcolor);
        }
    }

    pos = &console->visible_lines[console->y].glyphs[console->y];
    drv->putch(console->driver, console->y, console->x, pos->c, pos->bgcolor, pos->fgcolor);
}

static void console_refresh(void* data)
{
    console_t* console = data;
    if (console->redraw)
    {
        scoped_irq_lock();
        redraw(console);
        console->redraw = false;
    }
}

static void ff(console_t* console)
{
    console->current_line++;
    console->x = 0;

    if (LINE_INDEX(console->current_line) + console->resy >= console->capacity)
    {
        extend(console);
    }

    console->visible_lines = console->current_line;
    console->y = 0;
    redraw(console);
}

static void lf(console_t* console)
{
    if (console->scroll_top != 0 || console->scroll_bottom != console->resy - 1)
    {
        scroll_down(console, console->scroll_top, 1);
        return;
    }

    console->current_line++;
    console->x = 0;

    if (LINE_INDEX(console->current_line) == console->capacity - 1)
    {
        extend(console);
    }

    if (console->y < console->resy - 1)
    {
        ++console->y;
    }
    else
    {
        scoped_irq_lock();
        console->visible_lines++;
        console->redraw = true;
        rtc_schedule(&console_refresh, console);
    }
}

static void position_next(console_t* console)
{
    if (console->x == console->resx - 1)
    {
        lf(console);
    }
    else
    {
        ++console->x;
    }
}

static void position_prev(console_t* console)
{
    if (!console->x)
    {
        return;
    }

    --console->x;
}

static void console_write_char(console_t* console, uint8_t c)
{
    glyph_t* cur = &console->current_line->glyphs[console->x];
    cur->c = c;
    cur->fgcolor = console->current_fgcolor;
    cur->bgcolor = console->current_bgcolor;

    if (!console->redraw)
    {
        console->driver->putch(console->driver, console->y, console->x, c, console->current_fgcolor, console->current_bgcolor);
    }

    position_next(console);
}

static void cr(console_t* console)
{
    console->x = 0;
}

static void tab(console_t* console)
{
    console_write_char(console, ' ');
    console_write_char(console, ' ');
    console_write_char(console, ' ');
    console_write_char(console, ' ');
}

static void sgr(console_t* console, int* params, size_t params_count)
{
    console_driver_t* drv = console->driver;
    drv->setsgr(drv, params, params_count, &console->current_fgcolor, &console->current_bgcolor);
}

static void line_nr_print(console_t* console)
{
    char buf[32];
    size_t pos;
    size_t nr = (console->visible_lines + console->y) - console->lines;

    snprintf(buf, sizeof(buf), " [%zu/%zu]", nr, LINE_INDEX(console->current_line));
    pos = console->resx - strlen(buf);

    for (size_t i = 0; i < strlen(buf); ++i, ++pos)
    {
        console->driver->putch(console->driver, 0, pos, buf[i], console->default_fgcolor, console->default_bgcolor);
    }
}

static void tmux_mode_scroll_page_up(console_t* console)
{
    console->visible_lines -= console->resy;

    if (console->visible_lines < console->lines)
    {
        console->y = 0;
        console->visible_lines = console->lines;
    }

    redraw(console);
}

static void tmux_mode_scroll_up(console_t* console)
{
    if (console->visible_lines == console->lines)
    {
        --console->y;
        if ((short)console->y < 0) console->y = 0;
        return;
    }

    if (console->y == 0)
    {
        --console->visible_lines;

        if (console->visible_lines < console->lines)
        {
            console->visible_lines = console->lines;
        }
    }
    else
    {
        --console->y;
    }

    redraw(console);
}

static void tmux_mode_scroll_page_down(console_t* console)
{
    console->visible_lines += console->resy;

    if (console->visible_lines + console->resy > console->current_line)
    {
        console->y = min(console->resy - 1, console->current_line - console->lines);
        console->visible_lines = console->current_line - console->y;
    }

    redraw(console);
}

static void tmux_mode_scroll_down(console_t* console)
{
    ++console->y;

    if (console->y >= console->resy)
    {
        console->y = console->resy - 1;
        console->visible_lines++;

        if (console->visible_lines + console->y >= console->current_line)
        {
            console->visible_lines = console->current_line - console->y;
        }
    }

    if (console->visible_lines + console->y > console->current_line)
    {
        --console->y;
        console->visible_lines = console->current_line - console->y;
    }

    redraw(console);
}

static void tmux_mode_scroll_to(console_t* console, line_t* line)
{
    console->visible_lines = line;
    redraw(console);
}

static void tmux_mode_move_cursor(console_t* console, int dir)
{
    switch (dir)
    {
        case 'A':
            tmux_mode_scroll_up(console);
            break;
        case 'B':
            tmux_mode_scroll_down(console);
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
}

static int clear_cmd(console_t* console, const char*[])
{
    for (size_t y = 0; y < console->capacity; ++y)
    {
        glyph_t* glyphs = console->lines[y].glyphs;
        for (size_t x = 0; x < console->resx; ++x)
        {
            GLYPH_CLEAR(&glyphs[x]);
        }
    }

    console->current_line = console->visible_lines = console->lines;
    console->x = console->y = 0;

    redraw(console);

    return 0;
}

static void tmux_mode_parse_command(console_t* console, size_t max_count, char** parameters, size_t* count)
{
    char* save_ptr;
    char* new_token;
    char* buf = console->command_buf;

    strcpy(console->command_buf, console->command);

    do
    {
        new_token = strtok_r(buf, " ", &save_ptr);
        parameters[(*count)++] = new_token;
    }
    while (new_token && *count < max_count);
}

static void tmux_mode_execute_command(console_t* console)
{
    size_t count = 0;
    char* parameters[5] = {};

    tmux_mode_parse_command(console, array_size(parameters) - 1, parameters, &count);

    for (const command_t* cmd = commands; cmd->name; ++cmd)
    {
        if (!strcmp(parameters[0] + 1, cmd->name))
        {
            cmd->fn(console, (const char**)parameters + 1);
            return;
        }
    }
}

static void tmux_mode_handle_command(console_t* console, char c)
{
    if (isprint(c))
    {
        console->command[console->command_it++] = c;
        if (console->command_it >= sizeof(console->command) - 1)
        {
            console->command_it = 0;
            console->tmux_state = 0;
            redraw(console);
        }
    }
    else
    {
        switch (c)
        {
            case 3:
            case '\e':
                console->command_it = 0;
                console->tmux_state = '[';
                break;
            case '\t':
                break;
            case '\r':
                tmux_mode_execute_command(console);
                console->command_it = 0;
                console->tmux_state = 0;
                break;
            case 0x7f:
            case '\b':
                if (console->command_it)
                {
                    console->command[--console->command_it] = 0;
                }
                break;
        }
    }
}

static void region_clear(console_t* console, int top, int bottom)
{
    line_t* lines = console->visible_lines;

    for (size_t y = (size_t)top; y <= (size_t)bottom; ++y)
    {
        for (size_t x = 0; x < console->resx; ++x)
        {
            GLYPH_CLEAR(&lines[y].glyphs[x]);
        }
    }

    redraw(console);
}

static void lines_swap(line_t* lhs, line_t* rhs)
{
    glyph_t* temp_glyphs = lhs->glyphs;

    lhs->glyphs = rhs->glyphs;
    rhs->glyphs = temp_glyphs;
}

static void scroll_up(console_t* console, size_t origin, size_t count)
{
    log_debug(DEBUG_CONSOLE, "scrolling up by %d lines; region: %u-%u", count, console->scroll_top, console->scroll_bottom);

    ULIMIT(count, console->scroll_bottom - origin + 1);

    if (console->visible_lines + origin + count >= console->lines + console->capacity)
    {
        extend(console);
    }

    region_clear(console, console->scroll_bottom - count + 1, console->scroll_bottom);

    for (size_t i = console->scroll_bottom; i >= origin + count; --i)
    {
        lines_swap(&console->visible_lines[i], &console->visible_lines[i - count]);
    }

    redraw(console);
}

static void scroll_down(console_t* console, size_t origin, size_t count)
{
    log_debug(DEBUG_CONSOLE, "scrolling down by %d lines; region: %u-%u", count, console->scroll_top, console->scroll_bottom);

    ULIMIT(count, console->scroll_bottom - origin + 1);

    if (console->visible_lines + console->scroll_bottom >= console->lines + console->capacity)
    {
        extend(console);
    }

    region_clear(console, origin, origin + count - 1);

    for (size_t i = origin; i <= console->scroll_bottom - count; ++i)
    {
        lines_swap(&console->visible_lines[i], &console->visible_lines[i + count]);
    }

    redraw(console);
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
                log_debug(DEBUG_CONSOLE, "tmux_state: from \'%c\' to %d", from, to); \
            } \
            else if (isprint(to)) \
            { \
                log_debug(DEBUG_CONSOLE, "tmux_state: from %d to \'%c\'", from, to); \
            } \
            __VA_ARGS__; \
            console->tmux_state = to; \
            goto finish; \
        } \
    } \
    while (0)

static void tmux_mode(console_t* console, tty_t* tty, int c)
{
    if (likely(!console->tmux_state))
    {
        return;
    }

    if (console->tmux_state == ':')
    {
        tmux_mode_handle_command(console, c);
        goto finish;
    }

    switch (c)
    {
        case 'g':
            TMUX_TRANSITION('[', 'g', {});
            TMUX_TRANSITION('g', '[',
                {
                    console->y = console->x = 0;
                    tmux_mode_scroll_to(console, console->lines);
                });
            break;
        case 'G':
            TMUX_TRANSITION('[', '[',
                {
                    console->y = console->x = 0;
                    tmux_mode_scroll_to(console, console->orig_visible_lines);
                });
            break;
        case '^':
            console->x = 0;
            break;
        case '[':
            TMUX_TRANSITION(TTY_SPECIAL_MODE, '[',
                {
                    console->saved_x = console->x;
                    console->saved_y = console->y;
                });
            TMUX_TRANSITION('\e', '{', {});
            break;
        case 'q':
            TMUX_TRANSITION('[', 0,
                {
                    tty->special_mode = 0;
                    console->visible_lines = console->orig_visible_lines;
                    console->orig_visible_lines = NULL;
                    console->x = console->saved_x;
                    console->y = console->saved_y;
                    redraw(console);
                });
            break;
        case '\e':
            TMUX_TRANSITION('[', '\e', {});
            break;
        case '5' ... '6':
            TMUX_TRANSITION('{', c, {});
            break;
        case 'A' ... 'D':
            if (console->tmux_state == '{')
            {
                tmux_mode_move_cursor(console, c);
                console->tmux_state = '[';
            }
            break;
        case '~':
            TMUX_TRANSITION('5', '[',
                {
                    tmux_mode_scroll_page_up(console);
                });
            TMUX_TRANSITION('6', '[',
                {
                    tmux_mode_scroll_page_down(console);
                });
            break;
        case ':':
            TMUX_TRANSITION(TTY_SPECIAL_MODE, ':',
                {
                    memset(console->command, 0, sizeof(console->command));
                    *console->command = c;
                    console->command_it = 1;
                });
            TMUX_TRANSITION('[', ':',
                {
                    memset(console->command, 0, sizeof(console->command));
                    *console->command = c;
                    console->command_it = 1;
                });
            break;
    }

finish:
    if (console->tmux_state && console->tmux_state != TTY_SPECIAL_MODE)
    {
        cursor_update(console);
        line_nr_print(console);
        if (console->command_it)
        {
            for (size_t i = 0; i < sizeof(console->command); ++i)
            {
                char c = console->command[i];
                if (!c)
                {
                    c = ' ';
                }
                console->driver->putch(console->driver, console->resy - 1, i, c, console->default_fgcolor, console->default_bgcolor);
            }
        }
    }

    if (!console->tmux_state)
    {
        tty->special_mode = 0;
    }
}

static void control(console_t* console, int c, int* movecsr)
{
    switch (c)
    {
        case '\b': // Backspace (BS)
        case 0x7f: // Delete (DEL)
            position_prev(console);
            *movecsr = 1;
            break;
        case '\e': // Escape (ESC)
            log_debug(DEBUG_CONSOLE, "switch to ESC");
            console->escape_state = ESC_START;
            return;
        case '\t': // Horizontal Tab (TAB)
            tab(console);
            *movecsr = 1;
            break;
        case '\r': // Carriage Return (CR)
            cr(console);
            *movecsr = 1;
            break;
        case '\f': // Form Feed (FF)
            ff(console);
            *movecsr = 1;
            break;
        case '\v': // Vertical Tab (VT)
        case '\n': // Line Feed (LF)
            lf(console);
            *movecsr = 1;
            break;
    }
    console->escape_state = 0;
}

static void cursor_set_position(console_t* console, int x, int y)
{
    LIMIT(x, 0, console->resx - 1);
    LIMIT(y, 0, console->resy - 1);
    console->x = x;
    console->y = y;
    console->current_line = &console->visible_lines[y];
}

static void csi(console_t* console, int c, int* movecsr)
{
    int* params = console->csi.params;

    switch (c)
    {
        case 'A': // Cursor Up Ps Times (default = 1) (CUU)
            DEFAULT_VALUE(params[0], 1);
            cursor_set_position(
                console,
                console->x,
                console->y - params[0]);
            *movecsr = 1;
            break;
        case 'B': // Cursor Down Ps Times (default = 1) (CUD)
            DEFAULT_VALUE(params[0], 1);
            cursor_set_position(
                console,
                console->x,
                console->y + params[0]);
            *movecsr = 1;
            break;
        case 'C': // Cursor Forward Ps Times (default = 1) (CUF)
            DEFAULT_VALUE(params[0], 1);
            cursor_set_position(
                console,
                console->x + params[0],
                console->y);
            *movecsr = 1;
            break;
        case 'D': // Cursor Backward Ps Times (default = 1) (CUB)
            DEFAULT_VALUE(params[0], 1);
            cursor_set_position(
                console,
                console->x - params[0],
                console->y);
            *movecsr = 1;
            break;
        case 'H': // Cursor Position [row;column] (default = [1,1]) (CUP)
            DEFAULT_VALUE(params[0], 1);
            DEFAULT_VALUE(params[1], 1);
            cursor_set_position(
                console,
                params[1] - 1,
                params[0] - 1);
            *movecsr = 1;
            break;
        case 'J': // Erase in Display (ED)
            switch (params[0])
            {
                case 0:
                    region_clear(console, console->y + 1, console->resy - 1);
                    break;
                case 1:
                    region_clear(console, 0, console->y - 1);
                    break;
                case 2:
                    region_clear(console, 0, console->resy - 1);
                    break;
                default:
                    log_info("unsupported param in ED: %u", params[0]);
            }
            break;
        case 'K': // Erase in Line (EL)
            current_line_clear(console, params[0]);
            break;
        case 'L': // Insert Ps Line(s) (default = 1) (IL)
            DEFAULT_VALUE(params[0], 1);
            scroll_up(console, console->y, params[0]);
            *movecsr = 1;
            break;
        case 'M': // Delete Ps Line(s) (default = 1) (DL)
            DEFAULT_VALUE(params[0], 1);
            scroll_down(console, console->y, params[0]);
            *movecsr = 1;
            break;
        case 'h':
            if (console->csi.prefix != '?')
            {
                goto unknown;
            }
            switch (params[0]) // DEC Private Mode Set (DECSET)
            {
                case 1: // Application Cursor Keys (DECCKM)
                case 12: // Start blinking cursor
                case 25: // Show cursor (DECTCEM)
                case 1004: // Send FocusIn/FocusOut events
                case 2004: // Set bracketed paste mode
                    break;
                case 1049: // Save cursor as in DECSC. After saving the cursor,
                           // switch to the Alternate Screen Buffer, clearing it first
                    // TODO
                    break;
                default:
                    goto unknown;
            }
            break;
        case 'l':
            if (console->csi.prefix != '?')
            {
                goto unknown;
            }
            switch (params[0]) // DEC Private Mode Reset (DECRST)
            {
                case 1: // Normal Cursor Keys (DECCKM)
                case 12: // Stop blinking cursor
                case 25: // Hide cursor (DECTCEM)
                case 1004: // Don't send FocusIn/FocusOut events
                case 2004: // Reset bracketed paste mode
                    break;
                case 1049: // Use Normal Screen Buffer and restore cursor as in DECRC
                    // TODO
                    break;
                default:
                    goto unknown;
            }
            break;
        case 'm': // Character Attributes (SGR)
            sgr(console, console->csi.params, console->csi.params_nr);
            break;
        case 'r': // Set Scrolling Region [top;bottom] (default = full size of
                  // window) (DECSTBM)
            if (console->csi.prefix)
            {
                goto unknown;
            }
            DEFAULT_VALUE(params[0], 1);
            DEFAULT_VALUE(params[1], console->resy);
            LIMIT(params[0], 0, console->resy);
            LIMIT(params[1], 0, console->resy);
            console->scroll_top = params[0] - 1;
            console->scroll_bottom = params[1] - 1;
            cursor_set_position(console, 0, console->scroll_top);
            *movecsr = 1;
            break;
        default:
        unknown:
            log_info("unsupported CSI: \"\\e[");
            if (console->csi.prefix)
            {
                log_continue("%c", console->csi.prefix);
            }
            for (size_t i = 0; i < console->csi.params_nr; ++i)
            {
                if (i > 0)
                {
                    log_continue(";");
                }
                log_continue("%u", console->csi.params[i]);
            }
            log_continue("%c\"", c);
            break;
    }
}

static void console_putch_internal(console_t* console, tty_t* tty, int c, int* movecsr)
{
    if (console->tmux_state)
    {
        tmux_mode(console, tty, c);
        return;
    }
    else if (BETWEEN(c, 0, 0x1f) || c == 0x7f || BETWEEN(c, 0x80, 0x9f))
    {
        control(console, c, movecsr);
        return;
    }
    else if (console->escape_state & ESC_START)
    {
        if (console->escape_state & ESC_CSI)
        {
            switch (c)
            {
                case '?':
                case '>':
                    console->csi.prefix = c;
                    return;
            }

            if (BETWEEN(c, 0x40, 0x7e))
            {
                ++console->csi.params_nr;
                csi(console, c, movecsr);
                console->escape_state = 0;
                return;
            }
            else if (c == ';' && console->csi.params_nr < PARAMS_SIZE - 1)
            {
                ++console->csi.params_nr;
                return;
            }
            else if (BETWEEN(c, '0', '9'))
            {
                console->csi.params[console->csi.params_nr] *= 10;
                console->csi.params[console->csi.params_nr] += c - '0';
                return;
            }
            else
            {
                log_info("unsupported char in CSI: \'%c\' (%x)", c ,c);
                console->escape_state = 0;
                return;
            }
        }
        else
        {
            switch (c)
            {
                case '[':
                    log_debug(DEBUG_CONSOLE, "switch to [");
                    console->escape_state |= ESC_CSI;
                    memset(&console->csi, 0, sizeof(console->csi));
                    return;
                case '=': // Enter alternate keypad mode
                case '>': // Exit alternate keypad mode
                    return;
                default:
                    log_info("unsupported char after ESC: %c", c);
                    return;
            }
        }
    }
    else if (c == TTY_SPECIAL_MODE)
    {
        if (console->tmux_state == 0)
        {
            console->tmux_state = TTY_SPECIAL_MODE;
            console->orig_visible_lines = console->visible_lines;
            tty->special_mode = 1;
        }
        return;
    }

    console_write_char(console, c);
    *movecsr = 1;
}

static void console_putch(tty_t* tty, int c)
{
    int movecsr = 0;
    console_t* console = tty->driver->driver_data;

    if (console->disabled)
    {
        return;
    }

    console_putch_internal(console, tty, c, &movecsr);

    if (movecsr)
    {
        cursor_update(console);
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
    glyph_t* glyphs;
    page_t* pages;
    size_t i, row, col;
    console_t* console;

    if (unlikely(errno = driver->init(driver, &row, &col)))
    {
        log_error("driver initialization failed with %d", errno);
        return errno;
    }

    size_t size = align(sizeof(console_t), 0x100) + sizeof(glyph_t) * col * INITIAL_CAPACITY + MAX_CAPACITY * sizeof(line_t);
    size_t needed_pages = page_align(size) / PAGE_SIZE;

    log_notice("size: %u x %u; need %u B (%u pages) for %u lines", col, row, size, needed_pages, INITIAL_CAPACITY);

    pages = page_alloc(needed_pages, PAGE_ALLOC_CONT);

    if (unlikely(!pages))
    {
        log_warning("cannot allocate pages for lines!");
        return -1;
    }

    console = page_virt_ptr(pages);
    lines = shift_as(line_t*, console, align(sizeof(console_t), 0x100));
    glyphs = shift_as(glyph_t*, lines, MAX_CAPACITY * sizeof(line_t));

    for (i = 0; i < INITIAL_CAPACITY; ++i)
    {
        lines[i].glyphs = glyphs + i * col;
    }

    memset(&lines[INITIAL_CAPACITY], 0, (MAX_CAPACITY - INITIAL_CAPACITY) * sizeof(line_t*));

    memset(console, 0, sizeof(*console));
    console->resx = col;
    console->resy = row;
    console->lines = lines;
    console->current_line = lines;
    console->visible_lines = lines;
    console->capacity = INITIAL_CAPACITY;
    console->driver = driver;
    driver->defcolor(driver, &console->current_fgcolor, &console->current_bgcolor);
    console->default_fgcolor = console->current_fgcolor;
    console->default_bgcolor = console->current_bgcolor;
    console->scroll_bottom = console->resy - 1;
    console->scroll_top = 0;

    for (i = 0; i < INITIAL_CAPACITY; ++i)
    {
        for (size_t x = 0; x < col; ++x)
        {
            GLYPH_CLEAR(&lines[i].glyphs[x]);
        }
    }

    tty_driver.driver_data = console;

    redraw(console);

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

    if (console->disabled)
    {
        return size;
    }

    for (size_t i = 0; i < size; ++i)
    {
        console_putch_internal(console, tty, (uint8_t)buffer[i], &movecsr);
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
                    redraw(console);
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

static int extend(console_t* console)
{
    page_t* pages;
    glyph_t* glyphs;
    size_t line_len = console->resx;
    size_t new_lines = console->capacity * 2;
    size_t max_capacity = MAX_CAPACITY - MAX_CAPACITY % console->resy;

    if (new_lines + console->capacity > max_capacity)
    {
        size_t offset = console->resy * 10;
        new_lines = max_capacity - console->capacity;

        if (new_lines < offset)
        {
            ASSERT(console->lines[console->capacity - 1].glyphs);
            ASSERT(!console->lines[console->capacity].glyphs);

            page_t* temp = page_alloc(1, 0);

            if (unlikely(!temp))
            {
                log_error("cannot allocate temp page");
                return -1;
            }

            memcpy(
                page_virt_ptr(temp),
                console->lines,
                offset * sizeof(line_t));

            memmove(
                console->lines,
                console->lines + offset,
                (console->capacity - offset) * sizeof(line_t));

            memcpy(
                console->lines + console->capacity - offset,
                page_virt_ptr(temp),
                offset * sizeof(line_t));

            console->current_line -= offset;
            console->visible_lines -= offset;

            for (size_t i = 0; i < offset; ++i)
            {
                line_t* line = &console->current_line[i];
                for (size_t x = 0; x < console->resx; ++x)
                {
                    GLYPH_CLEAR(&line->glyphs[x]);
                }
            }

            return 0;
        }
    }

    size_t size = sizeof(glyph_t) * line_len * new_lines;
    size_t needed_pages = page_align(size) / PAGE_SIZE;

    log_debug(DEBUG_CONSOLE, "allocating %u B (%u pages) for %u more lines", size, needed_pages, new_lines);

    pages = page_alloc(needed_pages, PAGE_ALLOC_CONT);

    if (unlikely(!pages))
    {
        log_warning("cannot allocate pages for lines!");
        return -ENOMEM;
    }

    glyphs = page_virt_ptr(pages);

    for (size_t i = 0; i < new_lines; ++i)
    {
        line_t* line = &console->lines[i + console->capacity];
        line->glyphs = glyphs + i * line_len;
        for (size_t x = 0; x < console->resx; ++x)
        {
            GLYPH_CLEAR(&line->glyphs[x]);
        }
    }

    console->capacity += new_lines;

    return 0;
}

UNMAP_AFTER_INIT int console_init(void)
{
    return tty_driver_register(&tty_driver);
}
