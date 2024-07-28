#include <arch/io.h>

#include <kernel/fs.h>
#include <kernel/tty.h>
#include <kernel/page.h>
#include <kernel/devfs.h>
#include <kernel/string.h>
#include <kernel/module.h>

#define PORT 0xe9
#define debugcon_send(c) outb(c, PORT)

KERNEL_MODULE(debugcon);
module_init(debugcon_init);
module_exit(debugcon_deinit);

static int debugcon_open(tty_t* tty, file_t* file);
static int debugcon_close(tty_t* tty, file_t* file);
int debugcon_write(tty_t* tty, const char* buffer, size_t size);
static void debugcon_putch(tty_t* tty, int c);

static tty_driver_t tty_driver = {
    .name = "debug",
    .major = MAJOR_CHR_DEBUG,
    .minor_start = 0,
    .num = 1,
    .driver_data = NULL,
    .open = &debugcon_open,
    .close = &debugcon_close,
    .write = &debugcon_write,
    .putch = &debugcon_putch,
    .initialized = 0,
};

UNMAP_AFTER_INIT static int debugcon_init()
{
    return tty_driver_register(&tty_driver);
}

static int debugcon_deinit()
{
    return 0;
}

static int debugcon_open(tty_t*, file_t*)
{
    return 0;
}

static int debugcon_close(tty_t*, file_t*)
{
    return 0;
}

int debugcon_write(tty_t*, const char* buffer, size_t size)
{
    size_t old = size;
    while (size--)
    {
        debugcon_send(*buffer++);
    }
    return old;
}

static void debugcon_putch(tty_t*, int c)
{
    debugcon_send(c);
}
