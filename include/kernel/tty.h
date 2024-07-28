#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <kernel/fifo.h>
#include <kernel/list.h>
#include <kernel/wait.h>
#include <kernel/termios.h>
#include <kernel/api/types.h>
#include <kernel/tty_driver.h>

#define TTY_BUFFER_SIZE  256
#define TTY_INITIALIZED  0x4215
#define TTY_SPECIAL_MODE -1

typedef struct tty tty_t;

struct tty
{
    unsigned short    major;
    unsigned short    minor;
    tty_driver_t*     driver;
    termios_t         termios;
    int               driver_special_key;
    int               special_mode;
    char*             ldisc_buf;
    char*             ldisc_current;
    int               sid;
    wait_queue_head_t wq;
    BUFFER_MEMBER_DECLARE(buf, TTY_BUFFER_SIZE);
    list_head_t       list_entry;
};

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

tty_t* tty_from_file(struct file* file);

void tty_write_to_all(const char* buffer, size_t len, tty_t* excluded);
void tty_write_to(tty_t* tty, const char* buffer, size_t len);
