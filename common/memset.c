#define FUNCNAME memset
#include <common/defs.h>

#if !IS_ARCH_DEFINED
void* NAME(memset)(void* ptr, int c, size_t size)
{
    char* str = ptr;
    while (size--)
    {
        *str++ = c;
    }
    return ptr;
}

POST(memset);
#endif
