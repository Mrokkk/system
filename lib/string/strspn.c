#include <string.h>

size_t strspn(const char* str, const char* accept)
{
    const char* s = str;
    char buf[256];

    __builtin_memset(buf, 0, 256);

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

size_t strcspn(const char* str, const char* reject)
{
    const char* s = str;
    char buf[256];

    __builtin_memset(buf, 0, 256);
    buf[0] = 1;

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
