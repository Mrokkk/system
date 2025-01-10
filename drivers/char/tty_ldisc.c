#include "tty_ldisc.h"

#include <kernel/signal.h>
#include <kernel/process.h>
#include <kernel/api/errno.h>

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

int tty_ldisc_read(tty_t* tty, file_t* file, char* buffer, size_t count)
{
    size_t i;
    int errno;
    termios_t* termios = &tty->termios;

    WAIT_QUEUE_DECLARE(q, process_current);

    for (i = 0; i < count; i++)
    {
        while (fifo_get(&tty->buf, &buffer[i]))
        {
            if (file->mode & O_NONBLOCK)
            {
                return i;
            }

            if ((errno = process_wait(&tty->wq, &q)))
            {
                buffer[0] = 0;
                return errno;
            }
        }

        if (!L_ICANON(tty))
        {
            return ++i;
        }
        else if (buffer[i] == '\n' && I_ICRNL(tty))
        {
            return ++i;
        }
        else if (buffer[i] == '\r')
        {
            return ++i;
        }
        else if (buffer[i] == termios->c_cc[VEOF])
        {
            return 0;
        }
    }

    if (i == 0 && file->mode & O_NONBLOCK)
    {
        return -EAGAIN;
    }

    return i;
}

int tty_ldisc_write(tty_t* tty, file_t*, const char* buffer, size_t count)
{
    return tty->driver->write(tty, buffer, count);
}

int tty_ldisc_poll(tty_t* tty, file_t*, short events, short* revents, wait_queue_head_t** head)
{
    if (events != POLLIN)
    {
        return -EINVAL;
    }
    if (fifo_empty(&tty->buf))
    {
        *head = &tty->wq;
    }
    else
    {
        *revents = POLLIN;
        *head = NULL;
    }
    return 0;
}

void tty_ldisc_putch(tty_t* tty, int c)
{
    int signal = 0;
    process_t* p;
    int special_mode = tty->special_mode;

    if (c == '\r' && I_ICRNL(tty))
    {
        c = '\n';
    }
    else if (c == '\f') // FIXME: should be removed; I must make Bash understand \f properly
    {
        tty->driver->putch(tty, 12);
    }
    else if (c == tty->driver_special_key)
    {
        tty->driver->putch(tty, TTY_SPECIAL_MODE);
        return;
    }

    // In special mode all input is directly forwarded to
    // underlying driver and does not reach process
    if (L_ECHO(tty) || special_mode)
    {
        tty->driver->putch(tty, c);
    }

    if (special_mode)
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
        tty_session_kill(tty, signal);
        return;
    }

    if (c == C_VERASE(tty) && L_ICANON(tty))
    {
        if (tty->ldisc_current > tty->ldisc_buf)
        {
            *tty->ldisc_current-- = 0;
        }
        return;
    }

    *tty->ldisc_current++ = c;

    if (c == '\n' || c == C_VEOF(tty) || !L_ICANON(tty))
    {
        for (char* buf = tty->ldisc_buf; buf < tty->ldisc_current; ++buf)
        {
            fifo_put(&tty->buf, *buf);
        }

        tty->ldisc_current = tty->ldisc_buf;

        if ((p = wait_queue_pop(&tty->wq)))
        {
            process_wake(p);
            if (p != process_current)
            {
                process_current->need_resched = true;
            }
        }
    }
}
