#include <common/defs.h>
#include "stpcpy.h"

char* NAME(strcpy)(char* dest, const char* src)
{
    stpcpy_impl(dest, src);
    return dest;
}

POST(strcpy);
