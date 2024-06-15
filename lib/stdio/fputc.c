#include <stdio.h>

#include "file.h"

int LIBC(fputc)(int c, FILE* stream)
{
    stdout->buffer.putc(&stream->buffer, c);
    return c;
}

int LIBC(putchar)(int c)
{
    return fputc(c, stdout);
}

LIBC_ALIAS(fputc);
LIBC_ALIAS(putchar);
WEAK_ALIAS(fputc, putc);
