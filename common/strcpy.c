#define FUNCNAME strcpy
#include <common/defs.h>
#include "stpcpy.h"

#if !IS_ARCH_DEFINED
char* NAME(strcpy)(char* dst, const char* src)
{
    stpcpy_impl(dst, src);
    return dst;
}

POST(strcpy);
#endif
