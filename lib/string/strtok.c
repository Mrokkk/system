#include <string.h>

char* LIBC(strtok)(char* str, const char* delim)
{
    static char* buf;
    return strtok_r(str, delim, &buf);
}

LIBC_ALIAS(strtok);
