#define FUNCNAME memcmp
#include <common/defs.h>

#if !IS_ARCH_DEFINED
int NAME(memcmp)(const void* s1, const void* s2, size_t n)
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

POST(memcmp);
#endif
