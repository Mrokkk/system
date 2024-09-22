#include <common/defs.h>
#include "mempcpy.h"

#ifdef __i386__
void* NAME(mempcpy)(void* dest, const void* src, size_t n)
{
    return mempcpy_impl(dest, src, n);
}

POST(mempcpy);
#endif
