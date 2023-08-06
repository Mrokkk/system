#pragma once

#include <kernel/time.h>

#define POLLIN      0x0001
#define POLLPRI     0x0002
#define POLLOUT     0x0004

#define FD_SIZE 32
#define FD_BITS (8 * sizeof(unsigned long))
#define FD_LONGS (FD_SIZE / FD_BITS)

struct fd_set
{
    unsigned long fd_bits[FD_LONGS];
};

typedef struct fd_set fd_set;

struct pollfd
{
    int fd;
    short events;
    short revents;
};

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, timeval_t* timeout);
int poll(struct pollfd* fds, unsigned long nfds, int timeout);
