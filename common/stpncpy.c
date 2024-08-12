#define FUNCNAME stpncpy
#include <common/defs.h>
#include "stpncpy.h"

#if !IS_ARCH_DEFINED
char* NAME(stpncpy)(char* dst, const char* src, size_t size)
{
    return stpncpy_impl(dst, src, size);
}

POST(stpncpy);
#endif
