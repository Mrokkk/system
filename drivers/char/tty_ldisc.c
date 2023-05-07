#include "tty_ldisc.h"

#include <kernel/process.h>

int tty_ldisc_open(tty_t* tty, file_t* file)
{
    int errno;

    tty->ldisc_buf = tty->ldisc_current = slab_alloc(128);

    if (unlikely(!tty->ldisc_buf))
    {
        return -ENOMEM;
    }

    if (unlikely(errno = tty->driver->open(tty, file)))
    {
        slab_free(tty->ldisc_buf, 128);
        return errno;
    }

    tty->driver->initialized = TTY_INITIALIZED;

    return 0;
}

int tty_ldisc_read(tty_t* tty, file_t*, char* buffer, size_t count)
{
    size_t i;
    flags_t flags;
    termios_t* termios = &tty->termios;

    WAIT_QUEUE_DECLARE(q, process_current);

    for (i = 0; i < count; i++)
    {
        while (fifo_get(&tty->buf, &buffer[i]))
        {
            irq_save(flags);
            wait_queue_push(&q, &tty->wq);
            process_wait2(process_current, flags);
        }

        if (buffer[i] == '\n')
        {
            return ++i;
        }
        else if (buffer[i] == termios->c_cc[VEOF])
        {
            return 0;
        }
    }
    return 0;
}

int tty_ldisc_write(tty_t* tty, file_t* file, const char* buffer, size_t count)
{
    return tty->driver->write(tty, file, buffer, count);
}

void tty_ldisc_putch(tty_t* tty, char c, int flag)
{
    int signal = 0;
    struct process* p;
    struct process* temp;
    termios_t* termios = &tty->termios;

    if (c == '\r' && I_ICRNL(tty))
    {
        c = '\n';
    }

    if (L_ECHO(tty))
    {
        tty->driver->putch(tty, c);
    }

    if (flag == TTY_DONT_PUT_TO_USER)
    {
        return;
    }

    if (c == C_VINTR(tty))
    {
        signal = SIGINT;
    }
    else if (c == C_VSUSP(tty))
    {
        signal = SIGTSTP;
    }

    if (signal && L_ISIG(tty))
    {
        tty->ldisc_current = tty->ldisc_buf;
        temp = wait_queue_front(&tty->wq);
        for_each_process(p)
        {
            if (p->sid == tty->sid)
            {
                if (p == temp)
                {
                    fifo_put(&tty->buf, '\n');
                    p = wait_queue_pop(&tty->wq);
                    process_wake(p);
                }
                do_kill(p, signal);
            }
        }

        if (process_current->sid != tty->sid)
        {
            *need_resched = true;
        }

        return;
    }

    if (c == termios->c_cc[VERASE])
    {
        if (tty->ldisc_current > tty->ldisc_buf)
        {
            *tty->ldisc_current-- = 0;
        }
        return;
    }

    *tty->ldisc_current++ = c;

    if (c == '\n' || c == termios->c_cc[VEOF])
    {
        for (char* buf = tty->ldisc_buf; buf < tty->ldisc_current; ++buf)
        {
            fifo_put(&tty->buf, *buf);
        }

        tty->ldisc_current = tty->ldisc_buf;

        if (!wait_queue_empty(&tty->wq))
        {
            p = wait_queue_pop(&tty->wq);
            process_wake(p);
            if (p != process_current)
            {
                *need_resched = true;
            }
        }
    }
}
