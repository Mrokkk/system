#pragma once

#include <stdint.h>
#include <kernel/fs.h>

#include "tty.h"

int tty_ldisc_open(tty_t* tty, file_t* file);
int tty_ldisc_read(tty_t* tty, file_t* file, char* buffer, size_t count);
int tty_ldisc_write(tty_t* tty, file_t* file, const char* buffer, size_t count);
int tty_ldisc_poll(tty_t* tty, file_t* file, short events, short* revents, wait_queue_head_t** head);
void tty_ldisc_putch(tty_t* tty, char c);
