#pragma once

#include <kernel/api/statvfs.h>

int statvfs(char const* path, struct statvfs* buf);
int fstatvfs(int fd, struct statvfs* buf);
