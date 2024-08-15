#include <stdio.h>

int LIBC(sscanf)(const char* restrict str, const char* restrict format, ...)
{
    NOT_IMPLEMENTED(-1, "%p, %s", str, format);
}

LIBC_ALIAS(sscanf);
