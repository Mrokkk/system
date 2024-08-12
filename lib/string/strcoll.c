#include <string.h>

int LIBC(strcoll)(const char* s1, const char* s2)
{
    return strcmp(s1, s2);
}

LIBC_ALIAS(strcoll);
