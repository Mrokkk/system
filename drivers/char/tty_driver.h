#pragma once

#include <stddef.h>
#include <kernel/dev.h>
#include <kernel/list.h>

struct tty;
struct file;

struct tty_driver
{
    const char* name;
    dev_t major;
    dev_t minor_start, num;
    void* driver_data;
    int initialized;
    int (*open)(struct tty* tty, struct file* file);
    int (*close)(struct tty* tty, struct file* file);
    int (*write)(struct tty* tty, struct file* file, const char* buf, size_t count);
    int (*ioctl)(struct tty* tty, unsigned long request, void* arg);
    void (*putch)(struct tty* tty, int c);
    list_head_t drivers;
};

typedef struct tty_driver tty_driver_t;
