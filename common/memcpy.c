#define FUNCNAME memcpy
#include <common/defs.h>
#include "mempcpy.h"

#if !IS_ARCH_DEFINED
void* NAME(memcpy)(void* dest, const void* src, size_t n)
{
    mempcpy_impl(dest, src, n);
    return dest;
}

POST(memcpy);
#endif
