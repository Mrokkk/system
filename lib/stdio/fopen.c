#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "file.h"

static int mode_parse(const char* mode)
{
    int omode, oflags;
    switch (*mode)
    {
        case 'r':
            omode = O_RDONLY;
            oflags = 0;
            break;
        case 'w':
            omode = O_WRONLY;
            oflags = O_CREAT | O_TRUNC;
            break;
        case 'a':
            omode = O_WRONLY;
            oflags = O_CREAT | O_APPEND;
            break;
        default:
            return -1;
    }

    for (int i = 0; i < 7; ++i)
    {
        switch (*++mode)
        {
            case '\0':
            case ',':
                break;
            case '+':
                omode = O_RDWR;
                continue;
            default:
                continue;
        }
        break;
    }

    return omode | oflags;
}

FILE* LIBC(fopen)(const char* pathname, const char* mode)
{
    int fd;
    FILE* file = NULL;
    int flags;

    VALIDATE_INPUT(pathname && mode, NULL);

    RETURN_ERROR_IF((flags = mode_parse(mode)) < 0, EINVAL, NULL);

    fd = SAFE_SYSCALL(open(pathname, flags, 0666), NULL);

    if (UNLIKELY(file_allocate(fd, flags, &file)))
    {
        close(fd);
        return NULL;
    }

    return file;
}

FILE* LIBC(fdopen)(int fd, const char* mode)
{
    int actual_flags;
    FILE* file = NULL;

    VALIDATE_INPUT(mode, NULL);

    actual_flags = SAFE_SYSCALL(fcntl(fd, F_GETFL), NULL);

    RETURN_ERROR_IF(mode_parse(mode) != actual_flags, EINVAL, NULL);

    if (UNLIKELY(file_allocate(fd, actual_flags, &file)))
    {
        return NULL;
    }

    return file;
}

FILE* LIBC(freopen)(const char* pathname, const char* mode, FILE* stream)
{
    int flags;

    VALIDATE_INPUT(FILE_CHECK(stream), NULL);

    if (!pathname)
    {
        RETURN_ERROR_IF((flags = mode_parse(mode)) < 0, EINVAL, NULL);
        stream->mode = flags;
        return stream;
    }

    fclose(stream);
    return fopen(pathname, mode);
}

LIBC_ALIAS(fopen);
LIBC_ALIAS(fdopen);
LIBC_ALIAS(freopen);
