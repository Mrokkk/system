#pragma once

#include <kernel/api/errno.h>

#define errno_get(data) \
    ({ \
        int __ret = (int)(data); \
        int __errno = 0; \
        if (unlikely(__ret < 0 && __ret >= -ERRNO_MAX)) \
        { \
            __errno = __ret; \
        } \
        __errno; \
    })
