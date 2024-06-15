#include <stdio.h>
#include <stdlib.h>

#include "file.h"

FILE* LIBC(fopen)(const char* pathname, const char* mode)
{
    int fd;
    FILE* file = NULL;

    VALIDATE_INPUT(pathname && mode, NULL);
    fd = SAFE_SYSCALL(open(pathname, O_RDONLY), NULL); // FIXME: parse mode

    file_allocate(fd, &file);

    return file;
}

FILE* LIBC(fdopen)(int fd, const char* mode)
{
    VALIDATE_INPUT(mode, NULL);

    FILE* file = NULL;

    file_allocate(fd, &file);

    return file;
}

LIBC_ALIAS(fopen);
LIBC_ALIAS(fdopen);
