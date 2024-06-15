#include <stdio.h>

#include "file.h"

int LIBC(fputs)(const char* s, FILE* file)
{
    VALIDATE_INPUT(FILE_CHECK(file) && s, EOF);

    int i;
    for (i = 0; *s; ++i)
    {
        stdout->buffer.putc(&file->buffer, *s++);
    }

    return i;
}

int LIBC(puts)(const char* s)
{
    VALIDATE_INPUT(s, EOF);

    int i;
    for (i = 0; *s; ++i)
    {
        stdout->buffer.putc(&stdout->buffer, *s++);
    }
    stdout->buffer.putc(&stdout->buffer, '\n');

    return i + 1;
}

LIBC_ALIAS(fputs);
LIBC_ALIAS(puts);
