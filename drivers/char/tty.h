#pragma once

#include <stdint.h>
#include <kernel/wait.h>
#include <kernel/types.h>
#include <kernel/buffer.h>

typedef struct tty
{
    struct wait_queue_head wq;
    BUFFER_MEMBER_DECLARE(buf, 256);
    void (*tty_putchar)(uint8_t c);
    int initialized;
} tty_t;

#define TTY_DONT_PUT_TO_USER 1

void tty_char_insert(char c, int flag);
