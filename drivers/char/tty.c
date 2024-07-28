#include <kernel/fs.h>
#include <kernel/dev.h>
#include <kernel/irq.h>
#include <kernel/tty.h>
#include <kernel/ctype.h>
#include <kernel/devfs.h>
#include <kernel/module.h>
#include <kernel/process.h>
#include <kernel/api/ioctl.h>

#include "console.h"
#include "tty_ldisc.h"

#undef log_fmt
#define log_fmt(fmt) "tty: " fmt

#define DEFAULT_C_CC \
    "\003"  /* VINTR    ^C */ \
    "\034"  /* VQUIT    ^\ */ \
    "\b"    /* VERASE  DEL */ \
    "\025"  /* VKILL    ^U */ \
    "\004"  /* VEOF     ^D */ \
    "\0"    /* VTIME    \0 */ \
    "\1"    /* VMIN     \1 */ \
    "\0"    /* VSWTC    \0 */ \
    "\021"  /* VSTART   ^Q */ \
    "\023"  /* VSTOP    ^S */ \
    "\032"  /* VSUSP    ^Z */ \
    "\0"    /* VEOL     \0 */ \
    "\022"  /* VREPRINT ^Q */ \
    "\017"  /* VDISCARD ^U */ \
    "\027"  /* VWERASE  ^W */ \
    "\026"  /* VLNEXT   ^V */ \
    "\0"    /* VEOL2    \0 */

#define DEFAULT_C_IFLAG (BRKINT | ISTRIP | ICRNL | IMAXBEL | IXON | IXANY)
#define DEFAULT_C_OFLAG (OPOST | ONLCR | XTABS)
#define DEFAULT_C_CFLAG (CREAD | CS7 | PARENB | HUPCL)
#define DEFAULT_C_LFLAG (ECHO | ICANON | ISIG | IEXTEN | ECHOE| ECHOKE | ECHOCTL)

static int tty_open(file_t* file);
static int tty_read(file_t* file, char* buffer, size_t size);
static int tty_write(file_t* file, const char* buffer, size_t size);
static int tty_ioctl(file_t* file, unsigned long request, void* arg);
static int tty_poll(file_t* file, short events, short* revents, wait_queue_head_t** head);

static file_operations_t fops = {
    .open = &tty_open,
    .read = &tty_read,
    .write = &tty_write,
    .ioctl = &tty_ioctl,
    .poll = &tty_poll,
};

module_init(tty_init);
module_exit(tty_deinit);
KERNEL_MODULE(tty);

LIST_DECLARE(ttys);
static tty_t earlycon;

UNMAP_AFTER_INIT static int tty_init()
{
    int errno;
    if (unlikely(errno = devfs_register("tty", MAJOR_CHR_TTYAUX, 0, &fops)))
    {
        log_warning("cannot register /dev/tty device");
    }
    console_init();
    return 0;
}

static int tty_deinit()
{
    return 0;
}

int tty_driver_register(tty_driver_t* drv)
{
    int errno;
    char namebuf[32];
    tty_t* new_tty;

    log_info("registering %s, num = %u", drv->name, drv->num);

    for (dev_t minor = drv->minor_start; minor < drv->minor_start + drv->num; ++minor)
    {
        // earlycon is handled differently, as it registers itself before paging and
        // allocators are init, so I need to use a statically allocated one
        new_tty = drv->major == MAJOR_CHR_EARLYCON
            ? &earlycon
            : alloc(tty_t);

        if (unlikely(!new_tty))
        {
            return -ENOMEM;
        }

        new_tty->sid = 0;
        new_tty->driver = drv;
        new_tty->major = drv->major;
        new_tty->minor = minor;
        new_tty->driver_special_key = -1;
        new_tty->special_mode = 0;
        new_tty->termios.c_iflag = DEFAULT_C_IFLAG;
        new_tty->termios.c_oflag = DEFAULT_C_OFLAG;
        new_tty->termios.c_cflag = DEFAULT_C_CFLAG;
        new_tty->termios.c_lflag = DEFAULT_C_LFLAG;
        memcpy((char*)new_tty->termios.c_cc, DEFAULT_C_CC, NCCS);

        fifo_init(&new_tty->buf);
        wait_queue_head_init(&new_tty->wq);

        list_init(&new_tty->list_entry);
        list_add_tail(&new_tty->list_entry, &ttys);

        // earlycon also is not exposed to the user
        if (drv->major == MAJOR_CHR_EARLYCON)
        {
            continue;
        }

        sprintf(namebuf, "%s%u", drv->name, minor);

        if (unlikely(errno = devfs_register(namebuf, drv->major, minor, &fops)))
        {
            log_error("failed to register %s: %d", namebuf, errno);
            delete(new_tty);
            continue;
        }
    }

    return 0;
}

static tty_t* tty_find(dev_t major, dev_t minor)
{
    tty_t* tty;
    list_for_each_entry(tty, &ttys, list_entry)
    {
        if (tty->major == major && tty->minor == minor)
        {
            return tty;
        }
    }
    return NULL;
}

static int tty_open(file_t* file)
{
    int major = MAJOR(file->inode->rdev);
    int minor = MINOR(file->inode->rdev);
    tty_t* tty = NULL;

    if (unlikely(major == MAJOR_CHR_TTYAUX))
    {
        list_for_each_entry(tty, &ttys, list_entry)
        {
            if (tty->sid && tty->sid == process_current->sid)
            {
                goto found;
            }
        }
        return -ENODEV;
    }
    else
    {
        tty = tty_find(major, minor);
    }

    if (unlikely(!tty))
    {
        return -ENODEV;
    }

found:
    if (unlikely(!tty->driver->open))
    {
        return -ENOSYS;
    }

    if (process_current->sid == process_current->pid)
    {
        tty->sid = process_current->pid;
    }

    if (tty->driver->initialized == TTY_INITIALIZED)
    {
        file->private = tty;
        return 0;
    }

    file->private = tty;

    return tty_ldisc_open(tty, file);
}

static int tty_read(file_t* file, char* buffer, size_t count)
{
    return tty_ldisc_read(file->private, file, buffer, count);
}

static int tty_write(file_t* file, const char* buffer, size_t count)
{
    return tty_ldisc_write(file->private, file, buffer, count);
}

static int tty_ioctl(file_t* file, unsigned long request, void* arg)
{
    tty_t* tty = file->private;

    switch (request)
    {
        case TCGETA:
        case TIOCGETA:
            memcpy(arg, &tty->termios, sizeof(tty->termios));
            return 0;
        case TCSETA:
            memcpy(&tty->termios, arg, sizeof(tty->termios));
            return 0;
    }

    if (unlikely(!tty->driver->ioctl))
    {
        return -ENOSYS;
    }

    return tty->driver->ioctl(tty, request, arg);
}

static int tty_poll(file_t* file, short events, short* revents, wait_queue_head_t** head)
{
    return tty_ldisc_poll(file->private, file, events, revents, head);
}

void tty_char_insert(tty_t* tty, char c)
{
    tty_ldisc_putch(tty, c);
}

void tty_string_insert(tty_t* tty, const char* string)
{
    for (; *string; ++string)
    {
        char c = *string;
        tty_ldisc_putch(tty, c);
    }
}

void tty_write_to_all(const char* buffer, size_t len, tty_t* excluded)
{
    tty_t* tty;

    list_for_each_entry(tty, &ttys, list_entry)
    {
        if (!tty->driver->initialized || tty == excluded)
        {
            continue;
        }

        tty->driver->write(tty, buffer, len);
    }
}

tty_t* tty_from_file(file_t* file)
{
    tty_t* tty;
    const dev_t major = MAJOR(file->inode->rdev);
    const dev_t minor = MINOR(file->inode->rdev);

    list_for_each_entry(tty, &ttys, list_entry)
    {
        if (tty->major == major && tty->minor == minor)
        {
            return tty;
        }
    }

    return NULL;
}

void tty_write_to(tty_t* tty, const char* buffer, size_t len)
{
    tty->driver->write(tty, buffer, len);
}
