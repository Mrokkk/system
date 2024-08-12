#include <common/defs.h>

#if !IS_ARCH_DEFINED
size_t NAME(strlcpy)(char* dst, const char* src, size_t size)
{
    size_t len = NAME(strlen)(src);

    if (UNLIKELY(len >= size))
    {
        if (size)
        {
            NAME(memcpy)(dst, src, size);
            dst[size - 1] = '\0';
        }
    }
    else
    {
        NAME(memcpy)(dst, src, len + 1);
    }

    return len;
}

POST(strlcpy);
#endif
