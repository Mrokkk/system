#include "tty.h"

#include <kernel/fs.h>
#include <kernel/irq.h>
#include <kernel/device.h>

#include "vga.h"
#include "serial.h"
#include "console.h"
#include "keyboard.h"

int tty_open();

static struct file_operations fops = {
    .open = &tty_open,
    .read = &keyboard_read,
    .write = &console_write,
};

module_init(tty_init);
module_exit(tty_deinit);
KERNEL_MODULE(tty);

int tty_init()
{
    keyboard_init();
    video_init();
    char_device_register(MAJOR_CHR_TTY, "tty", &fops, 0, NULL);
    return 0;
}

int tty_deinit()
{
    return 0;
}

int tty_open(struct file*)
{
    console_init();
    return 0;
}
