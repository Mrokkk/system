#pragma once

#include <kernel/api/time.h>
#include <kernel/api/signal.h>

#define POLLIN      0x0001
#define POLLPRI     0x0002
#define POLLOUT     0x0004
#define POLLWRNORM  POLLOUT
#define POLLWRBAND  0x0008
#define POLLERR     0x0010
#define POLLHUP     0x0020
#define POLLNVAL    0x0040

#define FD_SETSIZE  32
#define FD_SIZE     32
#define FD_BITS     (8 * sizeof(long))
#define FD_LONGS    (FD_SIZE / FD_BITS)

struct fd_set
{
    long fd_bits[FD_LONGS];
};

struct timespec;
typedef struct fd_set fd_set;

struct pollfd
{
    int fd;
    short events;
    short revents;
};

int pselect(
    int nfds,
    fd_set* restrict readfds,
    fd_set* restrict writefds,
    fd_set* restrict errorfds,
    const struct timespec* restrict timeout,
    const sigset_t* restrict sigmask);

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout);
int poll(struct pollfd* fds, unsigned long nfds, int timeout);
