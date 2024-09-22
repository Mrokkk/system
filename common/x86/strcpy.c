#include <common/defs.h>
#include "stpcpy.h"

#ifdef __i386__
char* NAME(strcpy)(char* dest, const char* src)
{
    stpcpy_impl(dest, src);
    return dest;
}

POST(strcpy);
#endif
