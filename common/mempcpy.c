#define FUNCNAME mempcpy
#include <common/defs.h>
#include "mempcpy.h"

#if !IS_ARCH_DEFINED
void* NAME(mempcpy)(void* dest, const void* src, size_t n)
{
    return mempcpy_impl(dest, src, n);
}

POST(mempcpy);
#endif
