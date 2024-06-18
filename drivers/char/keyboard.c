#define log_fmt(fmt) "kbd: " fmt
#include <arch/i8042.h>
#include <kernel/irq.h>
#include <kernel/wait.h>
#include <kernel/device.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/process.h>
#include <kernel/termios.h>
#include <kernel/spinlock.h>

#include "tty.h"

#define KBD_HIGHEST_RATE    0x00

#define L_CTRL      0x1d
#define L_ALT       0x38
#define L_SHIFT     0x2a
#define R_SHIFT     0x36

static int shift = 0;
static char ctrl = 0;
static char alt = 0;
static char e0 = 0;

static void s_shift_up()
{
    shift = 1;
}

static void s_shift_down()
{
    shift = 0;
}

static void s_ctrl_up()
{
    ctrl = 1;
}

static void s_ctrl_down()
{
    ctrl = 0;
}

static void s_alt_up()
{
    alt = 1;
}

static void s_alt_down()
{
    alt =  0;
}

static tty_t* kb_tty;

typedef void (*action_t)();

#define scancode_action(code, action) \
    [code] = action##_up, \
    [code + 0x80] = action##_down

static action_t special_scancodes[] = {
    scancode_action(L_CTRL, s_ctrl),
    scancode_action(L_SHIFT, s_shift),
    scancode_action(R_SHIFT, s_shift),
    scancode_action(L_ALT, s_alt),
};

static const char* scancodes[] = {
    "\0\0",
    "\e\0",
    "1!", "2@", "3#", "4$", "5%", "6^", "7&", "8*", "9(", "0)", "-_", "=+", "\b\b",
    "\t\t", "qQ", "wW", "eE", "rR", "tT", "yY", "uU", "iI", "oO", "pP", "[{", "]}",
    "\r\r", "\0\0",
    "aA", "sS", "dD", "fF", "gG", "hH", "jJ", "kK", "lL", ";:", "'\"",
    "`~", "\0\0",
    "\\|",
    "zZ", "xX", "cC", "vV", "bB", "nN", "mM", ",<", ".>", "/?", "\0\0",
    "**",
    "\0\0", "  ", "\0\0"
};

static inline void keyboard_enable(void)
{
    i8042_send_cmd(I8042_KBD_PORT_ENABLE);
}

static inline void keyboard_disable(void)
{
    i8042_send_cmd(I8042_KBD_PORT_DISABLE);
}

static inline void keyboard_scancode_handle(uint8_t scan_code)
{
    char c = 0;

    if (special_scancodes[scan_code] && scan_code <= L_ALT+0x80)
    {
        special_scancodes[scan_code]();
        return;
    }

    if (scan_code == 0xe0)
    {
        e0 = 1;
        return;
    }
    else if (e0)
    {
        switch (scan_code)
        {
            case 0x49:
                tty_string_insert(kb_tty, "\e[5~");
                break;
            case 0x51:
                tty_string_insert(kb_tty, "\e[6~");
                break;
            case 0x48:
                tty_string_insert(kb_tty, "\e[A");
                break;
            case 0x50:
                tty_string_insert(kb_tty, "\e[B");
                break;
            case 0x4b:
                ctrl
                    ? tty_string_insert(kb_tty, "\e[1;5D")
                    : tty_string_insert(kb_tty, "\e[D");
                break;
            case 0x4d:
                ctrl
                    ? tty_string_insert(kb_tty, "\e[1;5C")
                    : tty_string_insert(kb_tty, "\e[C");
                break;
            case 0x47:
                tty_string_insert(kb_tty, "\e[1~");
                break;
            case 0x4f:
                tty_string_insert(kb_tty, "\e[4~");
                break;
            case 0x53:
                tty_string_insert(kb_tty, "\e[3~");
                break;
        }
        e0 = 0;
        return;
    }

    // TODO: Support more scancodes
    if (scan_code > 0x39)
    {
        return;
    }

    if (!(c = scancodes[scan_code][shift]))
    {
        return;
    }

    c = ctrl ? c & 0x1f : c;

    if (c == '\b')
    {
        c = kb_tty->termios.c_cc[VERASE];
    }

    if (alt)
    {
        tty_char_insert(kb_tty, '\e');
    }
    tty_char_insert(kb_tty, c);
}

void keyboard_irs()
{
    uint8_t scancode;

    keyboard_disable();

    scancode = i8042_receive();

    log_debug(DEBUG_KEYBOARD, "%x", scancode);
    keyboard_scancode_handle(scancode);

    keyboard_enable();
}

int keyboard_init(tty_t* tty)
{
    uint8_t byte;

    if (!i8042_is_detected(KEYBOARD))
    {
        log_warning("no keyboard detected");
        return -ENODEV;
    }

    if (i8042_config_set(KBD_EKI | KBD_SYS | KBD_KCC))
    {
        log_warning("seting config failed");
        return -ENODEV;
    }

    if ((byte = i8042_send_and_receive(I8042_RATE_SET, I8042_KBD_PORT)) != I8042_RESP_ACK)
    {
        log_warning("1: setting rate failed: %x", byte);
    }

    if ((byte = i8042_send_and_receive(KBD_HIGHEST_RATE, I8042_KBD_PORT)) != I8042_RESP_ACK)
    {
        log_warning("2: setting rate failed: %x", byte);
    }

    keyboard_enable();

    kb_tty = tty;

    irq_register(1, keyboard_irs, "keyboard", IRQ_DEFAULT);

    return 0;
}
