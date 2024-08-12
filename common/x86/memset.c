#include <common/defs.h>
#include "bzero.h"

void* NAME(memset)(void* s, int c, size_t count)
{
    int temp;

    if (c == 0)
    {
        bzero_impl(s, count);
        return s;
    }

    asm volatile(
        "rep; stosb;"
        : "=D" (temp)
        : "a" (c), "D" (s), "c" (count)
        : "memory");
    return s;
}

POST(memset);
