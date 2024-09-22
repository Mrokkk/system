#include <common/defs.h>
#include "bzero.h"

#ifdef __i386__
void NAME(bzero)(void* s, size_t count)
{
    bzero_impl(s, count);
}

POST(bzero);
#endif
