#include <arch/io.h>

#include <kernel/irq.h>
#include <kernel/wait.h>
#include <kernel/buffer.h>
#include <kernel/device.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/process.h>
#include <kernel/spinlock.h>

#include "console.h"

#define CMD_PORT                        0x64
#define DATA_PORT                       0x60
#define STATUS_PORT                     0x64

#define CMD_ENABLE_FIRST                0xae
#define CMD_DISABLE_FIRST               0xad
#define CMD_DISABLE_SECOND              0xa7
#define CMD_TEST_CONTROLLER             0xaa
#define CMD_TEST_FIRST_PS2_PORT         0xab
#define CMD_READ_CONFIGURATION_BYTE     0x20
#define CMD_WRITE_CONFIGURATION_BYTE    0x60

#define keyboard_disable() \
    do { keyboard_send_command(CMD_DISABLE_FIRST); keyboard_wait(); } while (0)

#define keyboard_enable() \
    keyboard_send_command(CMD_ENABLE_FIRST)

#define L_CTRL  0x1d
#define L_ALT   0x38
#define L_SHIFT 0x2a
#define R_SHIFT 0x36

static int shift = 0;
static char ctrl = 0;
static char alt = 0;
static char special = 0;

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

typedef void (*saction_t)();

#define scancode_action(code, action) \
    [code] = action##_up, \
    [code + 0x80] = action##_down,

saction_t special_scancodes[] = {
        [L_CTRL] = &s_ctrl_up,
        [L_CTRL+0x80] = &s_ctrl_down,
        [L_SHIFT] = &s_shift_up,
        [L_SHIFT+0x80] = &s_shift_down,
        [R_SHIFT] = &s_shift_up,
        [R_SHIFT+0x80] = &s_shift_down,
        [L_ALT] = &s_alt_up,
        [L_ALT+0x80] = &s_alt_down,
};

// It's not the nicest looking thing, but does its job ...
char *scancodes[] = {
        "\0\0", "\0\0",
        "1!", "2@", "3#", "4$", "5%", "6^", "7&", "8*", "9(", "0)", "-_", "=+", "\b\b",
        "\t\t", "qQ", "wW", "eE", "rR", "tT", "yY", "uU", "iI", "oO", "pP", "[{", "]}",
        "\n\n", "\0\0",
        "aA", "sS", "dD", "fF", "gG", "hH", "jJ", "kK", "lL", ";:", "'\"",
        "`~", "\0\0",
        "\\|",
        "zZ", "xX", "cC", "vV", "bB", "nN", "mM", ",<", ".>", "/?", "\0\0",
        "**",
        "\0\0", "  ", "\0\0"
};

BUFFER_DECLARE(keyboard_buffer, 32);
WAIT_QUEUE_HEAD_DECLARE(keyboard_wq);

static void keyboard_wait(void)
{
    for (int i = 0; i < 10000; i++)
    {
        if (!(inb(STATUS_PORT) & 0x2)) return;
    }

    log_warning("keyboard waiting timeout");
}

static void keyboard_send_command(uint8_t byte)
{
    keyboard_wait();
    outb(byte, CMD_PORT);
    io_wait();
}

static uint8_t keyboard_receive(void)
{
    while ((inb(STATUS_PORT) & 0x1) != 1);
    return inb(DATA_PORT);
}

int keyboard_read(
    struct file*,
    char* buffer,
    int size)
{
    int i;
    WAIT_QUEUE_DECLARE(kb, process_current);

    flags_t flags;
    irq_save(flags);

    for (i = 0; i < size; i++)
    {
        while (buffer_get(&keyboard_buffer, &buffer[i]))
        {
            wait_queue_push(&kb, &keyboard_wq);
            process_wait(process_current, flags);
        }
        if (buffer[i] == '\n')
        {
            ++i;
            goto finish;
        }
        else if (buffer[i] == 4)
        {
            i = 0;
            goto finish;
        }
        else if (buffer[i] == 3)
        {
            do_kill(process_current, SIGINT);
            i = 1;
            buffer[0] = 0;
            goto finish;
        }
        else if (buffer[i] == '\b' && i > 0)
        {
            buffer[i--] = 0;
            buffer[i--] = 0;
        }
    }

finish:
    return i;
}

void keyboard_irs()
{
    uint8_t scan_code = keyboard_receive();
    char character = 0;

    keyboard_disable();

    log_debug(DEBUG_KEYBOARD, "%x", scan_code);

    // Check if we have a defined action for this scancode
    if (special_scancodes[scan_code] && scan_code <= L_ALT+0x80)
    {
        special_scancodes[scan_code]();
        goto end;
    }

    if (scan_code == 0xe0)
    {
        special = 1;
        goto end;
    }
    else if (special && scan_code == 0x49)
    {
        log_debug(DEBUG_KEYBOARD, "page up pressed");
        console_putch('\e');
        console_putch('[');
        console_putch('5');
        console_putch('~');
        special = 0;
        goto end;
    }
    else if (special && scan_code == 0x51)
    {
        log_debug(DEBUG_KEYBOARD, "page down pressed");
        console_putch('\e');
        console_putch('[');
        console_putch('6');
        console_putch('~');
        special = 0;
        goto end;
    }

    // TODO: Number of implemented scancodes
    if (scan_code > 0x39)
    {
        goto end;
    }

    character = scancodes[scan_code][shift];
    if (ctrl && character)
    {
        if (character == 'c')
        {
            buffer_put(&keyboard_buffer, 3);
            goto wake;
        }
        else if (character == 'd')
        {
            buffer_put(&keyboard_buffer, 4);
            goto wake;
        }
    }
    else
    {
        buffer_put(&keyboard_buffer, character);
        //  FIXME: printing shouldn't be here; bug: if I press key on early startup, it crashes kernel
        console_putch(character);

wake:
        if (!wait_queue_empty(&keyboard_wq))
        {
            struct process *proc = wait_queue_pop(&keyboard_wq);
            process_wake(proc);
        }
    }

end:
    keyboard_enable();
}

int keyboard_init(void)
{
    uint8_t byte;

    keyboard_disable();
    keyboard_send_command(CMD_DISABLE_SECOND);

    while (inb(STATUS_PORT) & 1)
    {
        inb(DATA_PORT);
    }

    io_wait();

    keyboard_send_command(CMD_TEST_CONTROLLER);
    byte = keyboard_receive();
    if (byte != 0x55)
    {
        return -1;
    }

    keyboard_send_command(CMD_READ_CONFIGURATION_BYTE);
    byte = keyboard_receive();

    // If translation is disabled, try to enable it
    if (!(byte & (1 << 6)))
    {
        byte |= (1 << 6);
        keyboard_send_command(CMD_WRITE_CONFIGURATION_BYTE);
        outb(byte, DATA_PORT);
        keyboard_wait();
        keyboard_send_command(CMD_READ_CONFIGURATION_BYTE);
        if (!(keyboard_receive() & (1 << 6)))
        {
            return -1;
        }
    }

    // Enable interrupt
    if (!(byte & 0x1))
    {
        byte |= 0x1;
        keyboard_send_command(CMD_WRITE_CONFIGURATION_BYTE);
        outb(byte, DATA_PORT);
        keyboard_wait();
    }

    keyboard_enable();

    irq_register(0x01, keyboard_irs, "keyboard");

    return 0;
}
