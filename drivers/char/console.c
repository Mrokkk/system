#define log_fmt(fmt) "console: " fmt
#include "console.h"

#include <stddef.h>
#include <stdint.h>

#include <arch/rtc.h>

#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/tty.h>
#include <kernel/ctype.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>
#include <kernel/string.h>
#include <kernel/signal.h>
#include <kernel/process.h>
#include <kernel/api/ioctl.h>
#include <kernel/page_alloc.h>
#include <kernel/framebuffer.h>

#include "keyboard.h"
#include "console_config.h"
#include "console_driver.h"

// References:
// https://invisible-island.net/xterm/ctlseqs/ctlseqs.html

#define DEBUG_CONSOLE         0
#define DEBUG_CON_SCROLL      0
#define INITIAL_CAPACITY      128
#define MAX_CAPACITY          4096
#define FONT_DIR              "/usr/share/kbd/consolefonts/"
#define FONT_EXTENSION        ".psfu"
#define DEFAULT_FONT_PATH     FONT_DIR "default8x16" FONT_EXTENSION
#define CONSOLE_CONFIG_PATH   "/etc/vconsole.conf"

static int console_open(tty_t* tty, file_t* file);
static int console_setup(tty_t* tty, console_driver_t* driver);
static int console_close(tty_t* tty, file_t* file);
static int console_write(tty_t* tty, const char* buffer, size_t size);
static int console_ioctl(tty_t* tty, unsigned long request, void* arg);
static void console_putch_internal(console_t* console, tty_t* tty, int c, int* movecsr);
static void console_putch(tty_t* tty, int c);
static void scroll_up(console_t* console, size_t origin, size_t count);
static void scroll_down(console_t* console, size_t origin, size_t count);
static int grow(console_t* console);
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

static LIST_DECLARE(driver_ops);
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
        _glyph->attr = console->current_attr; \
        _glyph->fgcolor = console->current_fgcolor; \
        _glyph->bgcolor = console->current_bgcolor; \
    }

static void cursor_update(console_t* console)
{
    glyph_t current = console->visible_lines[console->y].glyphs[console->x];
    current.attr ^= GLYPH_ATTR_INVERSED;
    console->driver->ops->glyph_draw(console->driver, console->x, console->y, &current);

    if (console->prev_x != console->x || console->prev_y != console->y)
    {
        glyph_t previous = console->visible_lines[console->prev_y].glyphs[console->prev_x];
        previous.attr ^= ~GLYPH_ATTR_INVERSED;
        console->driver->ops->glyph_draw(console->driver, console->prev_x, console->prev_y, &previous);

        console->prev_x = console->x;
        console->prev_y = console->y;
    }
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
        drv->ops->glyph_draw(console->driver, x, console->y, glyph);
    }
}

static void redraw(console_t* console)
{
    size_t x, y;
    glyph_t* pos;
    line_t* temp;
    console_driver_t* drv = console->driver;
    glyph_t cursor = console->visible_lines[console->y].glyphs[console->x];
    cursor.attr ^= GLYPH_ATTR_INVERSED;

    for (y = 0, temp = console->visible_lines; y < console->resy; ++y, temp++)
    {
        for (pos = temp->glyphs, x = 0; x < console->resx; ++pos, ++x)
        {
            drv->ops->glyph_draw(console->driver, x, y, pos);
        }
    }

    pos = &console->visible_lines[console->y].glyphs[console->y];
    drv->ops->glyph_draw(console->driver, console->x, console->y, &cursor);
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

static void console_refresh_schedule(console_t* console)
{
    scoped_irq_lock();

    if (console->redraw)
    {
        return;
    }

    console->redraw = true;

    int id = rtc_event_schedule(&console_refresh, console);

    if (unlikely(id < 0))
    {
        log_error("cannot schedule refresh event");
        redraw(console);
        return;
    }

    console->rtc_event_id = id;
}

static void ff(console_t* console)
{
    console->current_line++;
    console->x = 0;

    if (LINE_INDEX(console->current_line) + console->resy >= console->capacity)
    {
        grow(console);
    }

    console->visible_lines = console->current_line;
    console->y = 0;

    console_refresh_schedule(console);
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
        grow(console);
    }

    if (console->y < console->resy - 1)
    {
        ++console->y;
    }
    else
    {
        if (console->alt_buffer_enabled)
        {
            console->current_line--;
            scroll_down(console, console->scroll_top, 1);
            return;
        }

        console->visible_lines++;

        console_refresh_schedule(console);
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

    cur->c       = c;
    cur->attr    = console->current_attr;
    cur->fgcolor = console->current_fgcolor;
    cur->bgcolor = console->current_bgcolor;

    if (!console->redraw)
    {
        console->driver->ops->glyph_draw(console->driver, console->x, console->y, cur);
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

static void rgb_set(console_driver_t* drv, int* params, uint32_t* color)
{
    if (drv->ops->sgr_rgb)
    {
        drv->ops->sgr_rgb(drv, (params[0] << 16) | (params[1] << 8) | (params[2]), color);
    }
}

static void color256_set(console_driver_t* drv, int value, uint32_t* color)
{
    if (drv->ops->sgr_256)
    {
        drv->ops->sgr_256(drv, value, color);
    }
    else if (drv->ops->sgr_16 && value < 16)
    {
        drv->ops->sgr_16(drv, value, color);
    }
    else if (drv->ops->sgr_8 && value < 8)
    {
        drv->ops->sgr_8(drv, value, color);
    }
}

static void sgr(console_t* console, int* params, size_t params_count)
{
    console_driver_t* drv = console->driver;

    for (size_t i = 0; i < params_count; ++i)
    {
        switch (params[i])
        {
            case 0: // Normal
                console->current_fgcolor = console->default_fgcolor;
                console->current_bgcolor = console->default_bgcolor;
                console->current_attr = 0;
                break;
            case 1: // Bold
            case 2: // Faint, decreased intensity
            case 3: // Italicized
                break;
            case 4: // Underlined
                console->current_attr |= GLYPH_ATTR_UNDERLINED;
                break;
            case 5: // Blink
                break;
            case 7: // Inverse
                console->current_attr |= GLYPH_ATTR_INVERSED;
                break;
            case 23: // Not italicized
                break;
            case 24: // Not underlined
                console->current_attr &= ~GLYPH_ATTR_UNDERLINED;
                break;
            case 27: // Positive (not inverse)
                console->current_attr &= ~GLYPH_ATTR_INVERSED;
                break;
            case 29: // Not crossed-out
                break;
            case 30 ... 37: // Set foreground color
                if (drv->ops->sgr_8)
                {
                    drv->ops->sgr_8(drv, params[i] - 30, &console->current_fgcolor);
                }
                break;
            case 38:
                switch (params[++i])
                {
                    case 2: // Set background color using RGB values
                        rgb_set(drv, params + ++i, &console->current_fgcolor);
                        break;
                    case 5: // Set background color using indexed color
                        color256_set(drv, params[++i], &console->current_fgcolor);
                        break;
                }
                return;
            case 39: // Set foreground color to default
                console->current_fgcolor = console->default_fgcolor;
                break;
            case 40 ... 47: // Set background color
                if (drv->ops->sgr_8)
                {
                    drv->ops->sgr_8(drv, params[i] - 40, &console->current_bgcolor);
                }
                break;
            case 48:
                switch (params[++i])
                {
                    case 2: // Set background color using RGB values
                        rgb_set(drv, params + ++i, &console->current_bgcolor);
                        break;
                    case 5: // Set background color using indexed color
                        color256_set(drv, params[i + 2], &console->current_bgcolor);
                        break;
                }
                return;
            case 49: // Set background color to default
                console->current_bgcolor = console->default_bgcolor;
                break;
            case 90 ... 97: // Set bright foreground color
                if (drv->ops->sgr_16)
                {
                    drv->ops->sgr_16(drv, params[i] - 90 + 8, &console->current_fgcolor);
                }
                break;
            case 100 ... 107: // Set bright background color
                if (drv->ops->sgr_16)
                {
                    drv->ops->sgr_16(drv, params[i] - 100 + 8, &console->current_bgcolor);
                }
                break;
            default:
                log_info("unsupported SGR: %u", params[i]);
        }
    }
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
        glyph_t glyph = {.c = buf[i], .fgcolor = console->default_fgcolor, .bgcolor = console->default_bgcolor};
        console->driver->ops->glyph_draw(console->driver, pos, 0, &glyph);
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

        redraw(console);
    }
    else
    {
        --console->y;
    }
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
        redraw(console);
    }
    else if (console->visible_lines + console->y > console->current_line)
    {
        --console->y;
        console->visible_lines = console->current_line - console->y;
        redraw(console);
    }
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

    console_refresh_schedule(console);

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
        buf = NULL;
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
            cmd->fn(console, (const char**)(&parameters[1]));
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
                console->tmux_state = 0;
                process_wake(console->kconsole);
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

    console_refresh_schedule(console);
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
        grow(console);
    }

    region_clear(console, console->scroll_bottom - count + 1, console->scroll_bottom);

    for (size_t i = console->scroll_bottom; i >= origin + count; --i)
    {
        lines_swap(&console->visible_lines[i], &console->visible_lines[i - count]);
    }

    console_refresh_schedule(console);
}

static void scroll_down(console_t* console, size_t origin, size_t count)
{
    log_debug(DEBUG_CONSOLE, "scrolling down by %d lines; region: %u-%u", count, console->scroll_top, console->scroll_bottom);

    ULIMIT(count, console->scroll_bottom - origin + 1);

    if (console->visible_lines + console->scroll_bottom >= console->lines + console->capacity)
    {
        grow(console);
    }

    region_clear(console, origin, origin + count - 1);

    for (size_t i = origin; i <= console->scroll_bottom - count; ++i)
    {
        lines_swap(&console->visible_lines[i], &console->visible_lines[i + count]);
    }

    console_refresh_schedule(console);
}

static void blank_insert(console_t* console, size_t count)
{
    ULIMIT(count, console->resx - console->x);

    line_t* line = console->current_line;
    size_t dest = console->x + count;
    size_t src = console->x;
    size_t size = console->resx - dest;

    memmove(&line->glyphs[dest], &line->glyphs[src], size * sizeof(*line->glyphs));

    for (size_t x = src; x < dest; ++x)
    {
        GLYPH_CLEAR(&line->glyphs[x]);
    }

    console_refresh_schedule(console);
}

static void char_delete(console_t* console, size_t count)
{
    ULIMIT(count, console->resx - console->x);

    line_t* line = console->current_line;
    size_t dest = console->x;
    size_t src = console->x + count;
    size_t size = console->resx - src;

    memmove(&line->glyphs[dest], &line->glyphs[src], size * sizeof(*line->glyphs));

    for (size_t x = console->resx - count; x < console->resx; ++x)
    {
        GLYPH_CLEAR(&line->glyphs[x]);
    }

    console_refresh_schedule(console);
}

static void alt_buffer_enable(console_t* console)
{
    if (console->alt_buffer_enabled)
    {
        return;
    }

    console->saved.x             = console->x;
    console->saved.y             = console->y;
    console->saved.scroll_top    = console->scroll_top;
    console->saved.scroll_bottom = console->scroll_bottom;
    console->saved.visible_lines = console->visible_lines;
    console->saved.current_line  = console->current_line;
    console->saved.lines         = console->lines;

    console->x                  = 0;
    console->y                  = 0;
    console->scroll_top         = 0;
    console->scroll_bottom      = console->resy - 1;
    console->lines              = console->alt_buffer;
    console->visible_lines      = console->alt_buffer;
    console->current_line       = console->alt_buffer;
    console->current_attr       = 0;
    console->current_fgcolor    = console->default_fgcolor;
    console->current_bgcolor    = console->default_bgcolor;
    console->alt_buffer_enabled = true;

    region_clear(console, 0, console->resy - 1);
}

static void alt_buffer_disable(console_t* console)
{
    if (!console->alt_buffer_enabled)
    {
        return;
    }

    console->x                  = console->saved.x;
    console->y                  = console->saved.y;
    console->scroll_top         = console->saved.scroll_top;
    console->scroll_bottom      = console->saved.scroll_bottom;
    console->visible_lines      = console->saved.visible_lines;
    console->current_line       = console->saved.current_line;
    console->lines              = console->saved.lines;
    console->current_attr       = 0;
    console->current_fgcolor    = console->default_fgcolor;
    console->current_bgcolor    = console->default_bgcolor;
    console->alt_buffer_enabled = false;

    console_refresh_schedule(console);
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
            glyph_t glyph = {.fgcolor = console->default_fgcolor, .bgcolor = console->default_bgcolor};
            for (size_t i = 0; i < sizeof(console->command); ++i)
            {
                char c = console->command[i];
                if (!c)
                {
                    c = ' ';
                }
                glyph.c = c;
                console->driver->ops->glyph_draw(console->driver, i, console->resy - 1, &glyph);
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
        case '@': // Insert Ps (Blank) Character(s) (default = 1) (ICH)
            DEFAULT_VALUE(params[0], 1);
            blank_insert(console, params[0]);
            break;
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
        case 'G': // Cursor Character Absolute  [column] (default = [row,1]) (CHA)
            DEFAULT_VALUE(params[0], 1);
            cursor_set_position(
                console,
                params[0] - 1,
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
        case 'P': // Delete Ps Character(s) (default = 1) (DCH)
            DEFAULT_VALUE(params[0], 1);
            char_delete(console, params[0]);
            break;
        case 'd': // Line Position Absolute  [row] (default = [1,column]) (VPA)
            DEFAULT_VALUE(params[0], 1);
            cursor_set_position(
                console,
                console->x,
                params[0] - 1);
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
                    alt_buffer_enable(console);
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
                    alt_buffer_disable(console);
                    break;
                default:
                    goto unknown;
            }
            break;
        case 'm': // Character Attributes (SGR)
            if (console->csi.prefix)
            {
                goto unknown;
            }
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
                case 'M': // Reverse Index (RI)
                    console->escape_state = 0;
                    if (console->y == console->scroll_top)
                    {
                        scroll_up(console, console->scroll_top, 1);
                    }
                    else
                    {
                        console->y--;
                    }
                    *movecsr = 1;
                    return;
                case '[': // Control Sequence Introducer (CSI)
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

static int console_driver_select(console_driver_t* driver)
{
    console_driver_ops_t* ops;

    list_for_each_entry(ops, &driver_ops, list_entry)
    {
        if (!ops->probe(&framebuffer))
        {
            log_info("selecting driver: %s", ops->name);
            driver->ops = ops;
            return 0;
        }
    }

    return -ENODEV;
}

static int console_open(tty_t* tty, file_t*)
{
    int errno;
    console_driver_t* driver;

    if (unlikely(!(driver = zalloc(console_driver_t))))
    {
        return -ENOMEM;
    }

    if (unlikely(errno = console_driver_select(driver)))
    {
        log_error("cannot initialize console");
        return errno;
    }

    if (unlikely(errno = keyboard_init(tty)))
    {
        log_warning("keyboard initialization failed with %d", errno);
    }

    if (unlikely(errno = console_setup(tty, driver)))
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

static void kconsole(console_t* console)
{
    flags_t flags;

    do_chroot(NULL);
    do_chroot("/root");

    while (1)
    {
        irq_save(flags);
        process_wait2(flags);

        if (console->command_it)
        {
            tmux_mode_execute_command(console);

            console->command_it = 0;
            memset(console->command, 0, sizeof(console->command));

            redraw(console);
        }
    }
}

static console_config_t console_config_read(void)
{
    int errno;
    console_config_t config = {};
    scoped_file_t* file = NULL;
    scoped_string_t* content = NULL;

    if (unlikely(errno = do_open(&file, CONSOLE_CONFIG_PATH, O_RDONLY, 0)))
    {
        log_warning("%s: open failed with %d", CONSOLE_CONFIG_PATH, errno);
        goto set;
    }

    if (unlikely(errno = string_read(file, 0, file->dentry->inode->size, &content)))
    {
        log_warning("%s: read failed with %d", CONSOLE_CONFIG_PATH, errno);
        goto set;
    }

    char* save_ptr;
    char* save_ptr2;
    char* line;
    char* key;
    char* value;
    size_t line_nr = 1;

    for (char* buf = content->data; buf || line; buf = NULL, line_nr++)
    {
        line = strtok_r(buf, "\n", &save_ptr);

        if (!line)
        {
            break;
        }

        key = strtok_r(line, "=", &save_ptr2);
        value = strtok_r(NULL, "=", &save_ptr2);

        if (unlikely(!value))
        {
            log_warning("%s:%u: not a key-value: \"%s\"", CONSOLE_CONFIG_PATH, line_nr, key);
            continue;
        }

        if (!strcmp(key, "FONT"))
        {
            size_t len = strlen(value) + sizeof(FONT_DIR FONT_EXTENSION) + 1;

            string_t* font_path = string_create(len);

            if (unlikely(!font_path))
            {
                log_warning("cannot allocate memory for font path; required size: %u", len);
                continue;
            }

            snprintf(font_path->data, len, FONT_DIR "%s" FONT_EXTENSION, value);

            config.font_path = font_path->data;
        }
        else
        {
            log_info("%s:%u: unsupported key: \"%s\"", CONSOLE_CONFIG_PATH, line_nr, key);
        }
    }

set:
    if (!config.font_path)
    {
        config.font_path = DEFAULT_FONT_PATH;
    }

    return config;
}

static int console_resize(console_t* console, size_t resx, size_t resy)
{
    page_t* pages;
    line_t* alt_lines;
    line_t* normal_lines;
    glyph_t* alt_glyphs;
    glyph_t* normal_glyphs;
    page_t* prev_pages = console->pages;
    size_t max_capacity = MAX_CAPACITY - MAX_CAPACITY % resy;
    size_t normal_size = page_align(sizeof(glyph_t) * resx * INITIAL_CAPACITY + sizeof(line_t) * max_capacity);
    size_t alt_size = page_align(sizeof(glyph_t) * resx * resy + sizeof(line_t) * resy);
    size_t size = normal_size + alt_size;
    size_t needed_pages = size / PAGE_SIZE;

    log_notice("size: %u x %u; need %u B (%u pages) for %u lines", resx, resy, size, needed_pages, INITIAL_CAPACITY);

    pages = page_alloc(needed_pages, PAGE_ALLOC_CONT);

    if (unlikely(!pages))
    {
        log_warning("cannot allocate pages for lines!");
        return -ENOMEM;
    }

    normal_lines = page_virt_ptr(pages);
    normal_glyphs = shift_as(glyph_t*, normal_lines, max_capacity * sizeof(line_t));

    alt_lines = page_virt_ptr(pages) + normal_size;
    alt_glyphs = shift_as(glyph_t*, alt_lines, resy * sizeof(line_t));

    for (size_t i = 0; i < INITIAL_CAPACITY; ++i)
    {
        normal_lines[i].glyphs = normal_glyphs + i * resx;
    }

    for (size_t i = 0; i < resy; ++i)
    {
        alt_lines[i].glyphs = alt_glyphs + i * resx;
    }

    memset(&normal_lines[INITIAL_CAPACITY], 0, (max_capacity - INITIAL_CAPACITY) * sizeof(line_t*));

    for (size_t i = 0; i < INITIAL_CAPACITY; ++i)
    {
        for (size_t x = 0; x < resx; ++x)
        {
            GLYPH_CLEAR(&normal_lines[i].glyphs[x]);
        }
    }

    for (size_t i = 0; i < resy; ++i)
    {
        for (size_t x = 0; x < resx; ++x)
        {
            GLYPH_CLEAR(&alt_lines[i].glyphs[x]);
        }
    }

    console->x             = 0;
    console->y             = 0;
    console->prev_x        = 0;
    console->prev_y        = 0;
    console->resx          = resx;
    console->resy          = resy;
    console->redraw        = false;
    console->normal_buffer = normal_lines;
    console->alt_buffer    = alt_lines;
    console->lines         = normal_lines;
    console->current_line  = normal_lines;
    console->visible_lines = normal_lines;
    console->capacity      = INITIAL_CAPACITY;
    console->max_capacity  = max_capacity;
    console->scroll_bottom = console->resy - 1;
    console->scroll_top    = 0;
    console->pages         = pages;

    if (prev_pages)
    {
        pages_free(prev_pages);
    }

    return 0;
}

static void console_notify(void* data)
{
    int errno;
    size_t resx, resy;
    console_t* console = data;
    console_driver_t* driver;

    if (unlikely(!(driver = zalloc(console_driver_t))))
    {
        log_error("%s: cannot allocate console_driver_t", __func__);
        return;
    }

    scoped_irq_lock();

    if (unlikely(errno = console_driver_select(driver)))
    {
        log_error("%s: cannot select console driver", __func__);
        return;
    }

    if (unlikely(errno = driver->ops->init(driver, &console->config, &resx, &resy)))
    {
        log_error("%s: driver initialization failed with %d", __func__, errno);
        return;
    }

    driver->ops->defcolor(driver, &console->current_fgcolor, &console->current_bgcolor);
    console->default_fgcolor = console->current_fgcolor;
    console->default_bgcolor = console->current_bgcolor;

    if (console->driver->ops->deinit)
    {
        console->driver->ops->deinit(console->driver);
    }
    delete(console->driver);

    console->driver = driver;

    if (driver->ops->screen_clear)
    {
        driver->ops->screen_clear(driver, console->default_bgcolor);
    }

    console_resize(console, resx, resy);

    redraw(console);
}

static int console_setup(tty_t* tty, console_driver_t* driver)
{
    int errno;
    size_t resx, resy;
    console_t* console;
    console_config_t config = console_config_read();

    if (unlikely(errno = driver->ops->init(driver, &config, &resx, &resy)))
    {
        log_error("driver initialization failed with %d", errno);
        return errno;
    }

    if (unlikely(!(console = zalloc(console_t))))
    {
        log_warning("cannot allocate console_t!");
        return -ENOMEM;
    }

    driver->ops->defcolor(driver, &console->current_fgcolor, &console->current_bgcolor);
    console->default_fgcolor = console->current_fgcolor;
    console->default_bgcolor = console->current_bgcolor;

    if (unlikely(errno = console_resize(console, resx, resy)))
    {
        delete(console);
        return errno;
    }

    console->driver = driver;
    console->kconsole = process_spawn("kconsole", &kconsole, console, SPAWN_KERNEL);
    memcpy(&console->config, &config, sizeof(config));

    if (console->driver->ops->screen_clear)
    {
        console->driver->ops->screen_clear(console->driver, console->default_bgcolor);
    }
    else
    {
        redraw(console);
    }

    tty->driver->driver_data = console;

    framebuffer_client_register(&console_notify, console);

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

    if (unlikely(console->disabled))
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

static int kdsetmode(console_t* console, long mode)
{
    switch (mode)
    {
        case KD_TEXT:
            console->disabled = false;

            if (console->driver->ops->screen_clear)
            {
                console->driver->ops->screen_clear(console->driver, console->default_bgcolor);
            }

            redraw(console);
            return 0;
        case KD_GRAPHICS:
            console->disabled = true;
            console_refresh(console);
            return 0;
    }

    return -EINVAL;
}

static int tiocgwinsz(console_t* console, winsize_t* w)
{
    w->ws_col = console->resx;
    w->ws_row = console->resy;
    return 0;
}

static int tiocswinsz(console_t* console, winsize_t* w)
{
    if (w->ws_col == console->resx && w->ws_row == console->resy)
    {
        return 0;
    }
    return -EINVAL;
}

static int kdfontop(console_t* console, tty_t* tty, console_font_op_t* op)
{
    int errno;
    console_driver_t* drv = console->driver;

    if (unlikely(!drv->ops->font_load))
    {
        return -ENOSYS;
    }

    if (unlikely(current_vm_verify(VERIFY_READ, op)))
    {
        return -EFAULT;
    }

    if (unlikely(!op->data))
    {
        return -EFAULT;
    }

    if (unlikely(current_vm_verify_buf(VERIFY_READ, op->data, op->size)))
    {
        return -EFAULT;
    }

    size_t resx, resy;

    console->disabled = true;

    if (unlikely(errno = drv->ops->font_load(drv, op->data, op->size, &resx, &resy)))
    {
        console->disabled = false;
        return errno;
    }

    if (resx != console->resx || resy != console->resy)
    {
        scoped_irq_lock();

        if (unlikely(errno = console_resize(console, resx, resy)))
        {
            // FIXME: new font is already loaded
            log_error("cannot resize: %d", errno);
            return errno;
        }

        if (console->rtc_event_id)
        {
            rtc_event_cancel(console->rtc_event_id);
            console->rtc_event_id = 0;
        }

        console->prev_x = 0;
        console->prev_y = 0;
        console->x      = 0;
        console->y      = 0;

        tty_session_kill(tty, SIGWINCH);
    }

    redraw(console);

    console->disabled = false;

    return 0;
}

static int console_ioctl(tty_t* tty, unsigned long request, void* arg)
{
    console_t* console = tty->driver->driver_data;

    switch (request)
    {
        case KDSETMODE:
            return kdsetmode(console, (long)arg);
        case TIOCGWINSZ:
            return tiocgwinsz(console, arg);
        case TIOCSWINSZ:
            return tiocswinsz(console, arg);
        case KDFONTOP:
            return kdfontop(console, tty, arg);
    }

    return -EINVAL;
}

static int grow(console_t* console)
{
    page_t* pages;
    glyph_t* glyphs;
    size_t line_len = console->resx;
    size_t new_lines = console->capacity * 2;
    size_t max_capacity = console->max_capacity;

    if (unlikely(console->alt_buffer_enabled))
    {
        panic("alt buffer enabled");
    }

    if (new_lines + console->capacity > max_capacity)
    {
        size_t offset = console->resy * 10;
        new_lines = max_capacity - console->capacity;

        if (new_lines < offset)
        {
            ASSERT(console->lines[console->capacity - 1].glyphs);

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

            pages_free(temp);

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

    pages_merge(pages, console->pages);

    console->capacity += new_lines;

    return 0;
}

UNMAP_AFTER_INIT int console_init(void)
{
    return tty_driver_register(&tty_driver);
}

int console_driver_register(console_driver_ops_t* ops)
{
    list_init(&ops->list_entry);
    list_add(&ops->list_entry, &driver_ops);
    return 0;
}
