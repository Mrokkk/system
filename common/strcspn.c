#define FUNCNAME strcspn
#include <common/defs.h>

#if !IS_ARCH_DEFINED
size_t NAME(strcspn)(const char* str, const char* reject)
{
    const char* s = str;
    char buf[256] = {1, };

    while (*reject)
    {
        buf[(unsigned char)*reject++] = 1;
    }

    while (!buf[(unsigned char)*s])
    {
        ++s;
    }

    return s - str;
}

POST(strcspn);
#endif
