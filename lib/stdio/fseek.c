#include <stdio.h>
#include <unistd.h>

#include "file.h"

int fseek(FILE* stream, long offset, int whence)
{
    VALIDATE_INPUT(FILE_CHECK(stream), -1);
    return lseek(stream->fd, offset, whence);
}

long ftell(FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream), -1);
    return lseek(stream->fd, 0, SEEK_CUR);
}
