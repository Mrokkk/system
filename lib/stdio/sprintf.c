#include <stdio.h>

int LIBC(sprintf)(char* buf, const char* fmt, ...)
{
    VALIDATE_INPUT(buf && fmt, -1);

    va_list args;
    int i;
    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);

    return i;
}

LIBC_ALIAS(sprintf);
