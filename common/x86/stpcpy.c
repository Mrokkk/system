#include <common/defs.h>
#include "stpcpy.h"

#ifdef __i386__
char* NAME(stpcpy)(char* dest, const char* src)
{
    return stpcpy_impl(dest, src);
}

POST(stpcpy);
#endif
