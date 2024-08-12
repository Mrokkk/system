#define FUNCNAME bzero
#include <common/defs.h>

#if !IS_ARCH_DEFINED
void NAME(bzero)(void* s, size_t count)
{
    char* str = s;
    while (count--)
    {
        *str++ = 0;
    }
}

POST(bzero);
#endif
