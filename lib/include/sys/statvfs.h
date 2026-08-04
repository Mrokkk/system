#pragma once

#include <sys/cdefs.h>
#include <kernel/api/statvfs.h>

__BEGIN_DECLS

int statvfs(char const* path, struct statvfs* buf);
int fstatvfs(int fd, struct statvfs* buf);

__END_DECLS
