#include <string.h>

char* LIBC(strcpy)(char* dst, const char* src)
{
    char* orig_dst = dst;
    while ((*dst++ = *src++) != 0);
    return orig_dst;
}

char* LIBC(strncpy)(char* dst, const char* src, size_t count)
{
    char* orig_dst = dst;
    while (count-- && (*dst++ = *src++) != 0);
    return orig_dst;
}

LIBC_ALIAS(strcpy);
LIBC_ALIAS(strncpy);
