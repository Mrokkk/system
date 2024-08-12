#include <string.h>

char* LIBC(strcat)(char* restrict dst, const char* restrict src)
{
    char* old_dst = dst;
    dst += strlen(dst);
    while ((*dst++ = *src++));
    *dst = 0;
    return old_dst;
}

char* LIBC(strncat)(char* restrict dst, const char* restrict src, size_t ssize)
{
    #define strnul(s)  (s + strlen(s))
    stpcpy(mempcpy(strnul(dst), src, strnlen(src, ssize)), "");
    return dst;
}

LIBC_ALIAS(strcat);
LIBC_ALIAS(strncat);
