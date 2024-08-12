#define FUNCNAME stpcpy
#include <common/defs.h>
#include "stpcpy.h"

#if !IS_ARCH_DEFINED
char* NAME(stpcpy)(char* dst, const char* src)
{
    return stpcpy_impl(dst, src);
}

POST(stpcpy);
#endif
