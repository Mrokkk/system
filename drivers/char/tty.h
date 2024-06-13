#pragma once

#include <stdint.h>
#include <kernel/fifo.h>
#include <kernel/list.h>
#include <kernel/wait.h>
#include <kernel/types.h>
#include <kernel/termios.h>

#include "tty_driver.h"

#define TTY_BUFFER_SIZE 256
#define TTY_INITIALIZED 0x4215

typedef struct tty
{
    dev_t major;
    tty_driver_t* driver;
    termios_t termios;
    int driver_special_key;
    int disabled;
    wait_queue_head_t wq;
    BUFFER_MEMBER_DECLARE(buf, TTY_BUFFER_SIZE);
    list_head_t list_entry;
    char* ldisc_buf;
    char* ldisc_current;
    int sid;
} tty_t;

#define I_ICRNL(tty)    ((tty)->termios.c_iflag & ICRNL)
#define L_ECHO(tty)     ((tty)->termios.c_lflag & ECHO)
#define L_ISIG(tty)     ((tty)->termios.c_lflag & ISIG)
#define L_ICANON(tty)   ((tty)->termios.c_lflag & ICANON)

#define C_VINTR(tty)    ((tty)->termios.c_cc[VINTR])
#define C_VSUSP(tty)    ((tty)->termios.c_cc[VSUSP])
#define C_VEOF(tty)     ((tty)->termios.c_cc[VEOF])
#define C_VERASE(tty)   ((tty)->termios.c_cc[VERASE])

int tty_driver_register(tty_driver_t* drv);
void tty_char_insert(tty_t* tty, char c);
void tty_string_insert(tty_t* tty, const char* string);
