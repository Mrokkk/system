#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS

struct utimbuf
{
    time_t actime;  // access time
    time_t modtime; // modification time
};

int utime(char const* pathname, const struct utimbuf* times);

__END_DECLS
