#include <string.h>

char* strcat(char* restrict dst, const char* restrict src)
{
    char* old_dst = dst;
    dst += strlen(dst);
    while ((*dst++ = *src++));
    *dst = 0;
    return old_dst;
}
