#pragma once

#include <common/defs.h>

static inline char* stpncpy_impl(char* dst, const char* src, size_t size)
{
    size_t src_len = NAME(strnlen)(src, size);

    if (src_len != size)
    {
        NAME(memset)(dst + src_len, 0, size - src_len);
    }

    NAME(memcpy)(dst, src, src_len);
    return dst + src_len;
}
