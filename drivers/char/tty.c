#include <kernel/device.h>
#include <kernel/fs.h>
#include <kernel/irq.h>

int tty_open();
int keyboard_init();
int keyboard_read(/* ... */);
int video_init();
int display_write(/* ... */);
void display_print(const char *text);

static struct file_operations fops = {
    .open = &tty_open,
    .read = &keyboard_read,
    .write = &display_write,
};

module_init(tty_init);
module_exit(tty_deinit);
KERNEL_MODULE(tty);

static char tty_stat;

/*===========================================================================*
 *                                  tty_init                                 *
 *===========================================================================*/
int tty_init() {

    keyboard_init();

    video_init();

    char_device_register(MAJOR_CHR_TTY, "tty0", &fops);

    console_register(&display_print);

    return 0;

}

/*===========================================================================*
 *                                 tty_deinit                                *
 *===========================================================================*/
int tty_deinit() {

    return 0;

}

/*===========================================================================*
 *                                  tty_open                                 *
 *===========================================================================*/
int tty_open(struct inode *inode, struct file *file) {
    (void)inode; (void)file;
    tty_stat = 1;
    return 0;
}

