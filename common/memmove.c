#define FUNCNAME memmove
#include <stdint.h>
#include <common/defs.h>

#if !IS_ARCH_DEFINED
void* NAME(memmove)(void* dest, const void* src, size_t n)
{
    if ((uintptr_t)(dest) - (uintptr_t)(src) >= n)
    {
        NAME(memcpy)(dest, src, n);
    }
    else
    {
        const char* src_it = (const char*)src + n;
        char* dst_it = (char*)dest + n;
        while (dst_it != dest)
        {
            *--dst_it = *--src_it;
        }
    }

    return dest;
}

POST(memmove);
#endif
