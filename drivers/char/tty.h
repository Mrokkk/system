#pragma once

#include <kernel/types.h>

struct tty;

typedef struct tty_driver
{
    unsigned short major;
    int (*read)(struct tty*, char*, int);
    int (*write)(struct tty*, char*, int);
} tty_driver_t;

typedef struct tty
{
    tty_driver_t driver;
} tty_t;
