#include <string.h>

int LIBC(memcmp)(const void* s1, const void* s2, size_t n)
{
    int res = 0;

    while (n--)
    {
        if ((res = *(uint8_t*)s1++ - *(uint8_t*)s2++) != 0)
        {
            break;
        }
    }

    return res;
}

LIBC_ALIAS(memcmp);
