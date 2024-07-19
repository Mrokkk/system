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

int LIBC(snprintf)(char* buf, size_t size, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int res = vsnprintf(buf, size, format, args);
    va_end(args);
    return res;
}

LIBC_ALIAS(sprintf);
LIBC_ALIAS(snprintf);
