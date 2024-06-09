#pragma once

#include <sys/types.h>

struct utimbuf
{
    time_t actime;  // access time
    time_t modtime; // modification time
};

int utime(char const* pathname, const struct utimbuf* times);
