#include <unistd.h>
#include <kernel/api/poll.h>

unsigned int LIBC(sleep)(unsigned int seconds)
{
    struct timespec timeout = {
        .tv_sec = seconds,
        .tv_nsec = 0
    };
    return pselect(0, NULL, NULL, NULL, &timeout, NULL);
}

int LIBC(usleep)(useconds_t usec)
{
    struct timespec timeout = {
        .tv_sec = usec / 1000000,
        .tv_nsec = (usec % 1000000) * 1000
    };
    return pselect(0, NULL, NULL, NULL, &timeout, NULL);
}

LIBC_ALIAS(sleep);
LIBC_ALIAS(usleep);
