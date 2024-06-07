#include <string.h>

void* memchr(const void* s, int c, size_t n)
{
    while (n--)
    {
        if (*(char*)s == c)
        {
            return (void*)s;
        }
        ++s;
    }

    return NULL;
}

void* memrchr(const void* s, int c, size_t n)
{
    if (UNLIKELY(n == 0))
    {
        return NULL;
    }

    s += n - 1;
    while (n--)
    {
        if (*(char*)s == c)
        {
            return (void*)s;
        }
        --s;
    }

    return NULL;
}
