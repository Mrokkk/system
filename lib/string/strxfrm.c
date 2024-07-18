#include <string.h>

size_t LIBC(strxfrm)(char* dest, char const* src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i]; ++i)
    {
        dest[i] = src[i];
    }
    for (; i < n; ++i)
    {
        dest[i] = '\0';
    }
    return i;
}

LIBC_ALIAS(strxfrm);
