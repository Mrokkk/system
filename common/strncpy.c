#define FUNCNAME strncpy
#include <common/defs.h>
#include "stpncpy.h"

#if !IS_ARCH_DEFINED
char* NAME(strncpy)(char* dst, const char* src, size_t size)
{
    stpncpy_impl(dst, src, size);
    return dst;
}

POST(strncpy);
#endif
