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
