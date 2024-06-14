#include <string.h>

void* LIBC(memmove)(void* dest, const void* src, size_t n)
{
    if (ADDR(dest) - ADDR(src) >= n)
    {
        memcpy(dest, src, n);
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

LIBC_ALIAS(memmove);
