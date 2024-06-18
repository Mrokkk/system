#include <stdio.h>

#include "file.h"

int LIBC(printf)(const char* fmt, ...)
{
    VALIDATE_INPUT(fmt, -1);

    va_list args;
    int printed;

    stdout->last = LAST_WRITE;

    va_start(args, fmt);
    printed = vsprintf_internal(&stdout->buffer, fmt, args);
    va_end(args);

    return printed;
}

int LIBC(fprintf)(FILE* file, const char* fmt, ...)
{
    VALIDATE_INPUT(file && fmt, -1);

    va_list args;
    int printed;

    file->last = LAST_WRITE;

    va_start(args, fmt);
    printed = vsprintf_internal(&file->buffer, fmt, args);
    va_end(args);

    return printed;
}

int LIBC(vfprintf)(FILE* restrict stream, const char* restrict format, va_list ap)
{
    VALIDATE_INPUT(FILE_CHECK(stream) && format, EOF);

    stream->last = LAST_WRITE;
    return vsprintf_internal(&stream->buffer, format, ap);
}

LIBC_ALIAS(printf);
LIBC_ALIAS(fprintf);
LIBC_ALIAS(vfprintf);
