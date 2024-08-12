#include <common/defs.h>
#include "bzero.h"

void NAME(bzero)(void* s, size_t count)
{
    bzero_impl(s, count);
}

POST(bzero);
