#pragma once

#include <sys/cdefs.h>
#include <kernel/api/poll.h>

__BEGIN_DECLS

void FD_SET(int fd, fd_set* fdset);
void FD_CLR(int fd, fd_set* fdset);
void FD_ZERO(fd_set* fdset);
int FD_ISSET(int fd, fd_set* fdset);

__END_DECLS
