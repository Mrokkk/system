#include <sys/poll.h>
#include <kernel/bitset.h>

void FD_SET(int fd, fd_set* fdset)
{
    bitset_set((uint32_t*)&fdset->fd_bits, fd);
}

void FD_CLR(int fd, fd_set* fdset)
{
    bitset_clear((uint32_t*)&fdset->fd_bits, fd);
}

void FD_ZERO(fd_set* fdset)
{
    bitset_initialize(fdset->fd_bits);
}

int FD_ISSET(int fd, fd_set* fdset)
{
    return bitset_test((uint32_t*)&fdset->fd_bits, fd);
}

int pselect(
    int nfds,
    fd_set* readfds,
    fd_set* writefds,
    fd_set* errorfds,
    const struct timespec* timeout,
    const sigset_t* sigmask)
{
    UNUSED(nfds); UNUSED(readfds); UNUSED(writefds); UNUSED(errorfds);
    UNUSED(timeout); UNUSED(sigmask);
    NOT_IMPLEMENTED(-1);
}
