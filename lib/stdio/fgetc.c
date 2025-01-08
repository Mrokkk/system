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
    int i;
    int ret = SAFE_SYSCALL(read(stream->fd, s, size - 1), NULL);

    // FIXME: implement proper buffering; below workaround will work
    // only for files
    for (i = 0; i < ret; ++i)
    {
        if (s[i] == '\n')
        {
            s[i + 1] = '\0';
            int cur = lseek(stream->fd, 0, SEEK_CUR);
            lseek(stream->fd, cur - (ret - i) + 1, SEEK_SET);
            break;
        }
    }

    if (ret == 0)
    {
        *s = '\0';
        return NULL;
    }
    s[ret] = '\0';
    return s;
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
