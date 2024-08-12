#include <common/defs.h>
#include "mempcpy.h"

void* NAME(memcpy)(void* dest, const void* src, size_t n)
{
    mempcpy_impl(dest, src, n);
    return dest;
}

POST(memcpy);
