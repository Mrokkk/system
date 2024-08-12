#include <common/defs.h>

static inline char* stpcpy_impl(char* dst, const char* src)
{
    while ((*dst++ = *src++) != 0);
    return dst - 1;
}
