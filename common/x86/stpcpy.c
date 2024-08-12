#include <common/defs.h>
#include "stpcpy.h"

char* NAME(stpcpy)(char* dest, const char* src)
{
    return stpcpy_impl(dest, src);
}

POST(stpcpy);
