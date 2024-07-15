#include <stdio.h>
#include <unistd.h>

#include "file.h"

int LIBC(fgetc)(FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream), EOF);

    char c;
    stream->last = LAST_READ;
    int ret = SAFE_SYSCALL(read(stream->fd, &c, 1), EOF);

    if (ret == 0)
    {
        return EOF;
    }

    return c;
}

char* LIBC(fgets)(char* s, int size, FILE* stream)
{
    NOT_IMPLEMENTED(NULL, "%p, %u, %p", s, size, stream);
}

int LIBC(ungetc)(int c, FILE* stream)
{
    NOT_IMPLEMENTED(EOF, "%u, %p", c, stream);
}

int LIBC(getchar)(void)
{
    return LIBC(fgetc)(stdin);
}

LIBC_ALIAS(fgetc);
LIBC_ALIAS(fgets);
LIBC_ALIAS(ungetc);
LIBC_ALIAS(getchar);
WEAK_ALIAS(fgetc, getc);
