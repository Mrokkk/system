#include <stdio.h>
#include <unistd.h>

#include "file.h"

int LIBC(fgetc)(FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream), EOF);

    char c;
    SAFE_SYSCALL(read(stream->fd, &c, 1), EOF);
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

LIBC_ALIAS(fgetc);
LIBC_ALIAS(fgets);
LIBC_ALIAS(ungetc);
