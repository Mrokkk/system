#include "tty.h"

#include <kernel/fs.h>
#include <kernel/irq.h>
#include <kernel/device.h>
#include <kernel/process.h>

#include "fbcon.h"
#include "egacon.h"
#include "serial.h"
#include "console.h"
#include "keyboard.h"
#include "framebuffer.h"

int tty_open(struct file* file);
int tty_read(struct file* file, char* buffer, int size);

#define TTY_INITIALIZED 0x4215

static struct file_operations fops = {
    .open = &tty_open,
    .read = &tty_read,
    .write = &console_write,
};

module_init(tty_init);
module_exit(tty_deinit);
KERNEL_MODULE(tty);

#define TTY_NR 1

tty_t ttys[TTY_NR];

int tty_init()
{
    char_device_register(MAJOR_CHR_TTY, "tty", &fops, 0, NULL);
    return 0;
}

int tty_deinit()
{
    return 0;
}

int tty_open(struct file*)
{
    int errno;
    console_driver_t* driver;

    if (ttys[0].initialized == TTY_INITIALIZED)
    {
        return 0;
    }

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
            break;
        case FB_TYPE_TEXT:
            driver->init = &egacon_init;
            driver->putch = &egacon_char_print;
            driver->setsgr = &egacon_setsgr;
            break;
    }

    if ((errno = console_init(driver)))
    {
        log_error("cannot initialize tty video driver");
        return -ENOSYS;
    }

    ttys[0].tty_putchar = &console_putch;
    buffer_init(&ttys[0].buf);
    wait_queue_head_init(&ttys[0].wq);

    keyboard_init();

    ttys[0].initialized = TTY_INITIALIZED;

    return 0;
}

int tty_read(
    struct file*,
    char* buffer,
    int size)
{
    int i;
    flags_t flags;
    tty_t* tty = ttys;
    WAIT_QUEUE_DECLARE(q, process_current);

    for (i = 0; i < size; i++)
    {
        while (buffer_get(&tty->buf, &buffer[i]))
        {
            irq_save(flags);
            wait_queue_push(&q, &tty->wq);
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

void tty_char_insert(char c, int flag)
{
    ttys[0].tty_putchar(c);
    if (flag != TTY_DONT_PUT_TO_USER)
    {
        buffer_put(&ttys[0].buf, c);
        if (!wait_queue_empty(&ttys[0].wq))
        {
            struct process* proc = wait_queue_pop(&ttys[0].wq);
            process_wake(proc);
        }
    }
}
