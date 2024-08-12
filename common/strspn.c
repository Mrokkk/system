#define FUNCNAME strspn
#include <common/defs.h>

#if !IS_ARCH_DEFINED
size_t NAME(strspn)(const char* str, const char* accept)
{
    const char* s = str;
    char buf[256] = {0, };

    while (*accept)
    {
        buf[(unsigned char)*accept++] = 1;
    }

    while (buf[(unsigned char)*s])
    {
        ++s;
    }

    return s - str;
}

POST(strspn);
#endif
