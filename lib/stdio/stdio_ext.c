#include <stdio.h>
#include <stdio_ext.h>
#include "file.h"

size_t __fbufsize(FILE* stream)
{
    return stream->buffer.end - stream->buffer.start;
}

size_t __fpending(FILE* stream)
{
    return stream->buffer.current - stream->buffer.start;
}

int __freading(FILE* stream)
{
    return (stream->mode & O_ACCMODE) == O_RDONLY || stream->last == LAST_READ;
}

int __fwriting(FILE* stream)
{
    return (stream->mode & O_ACCMODE) == O_WRONLY || stream->last == LAST_WRITE;
}

int __fmode(FILE* stream)
{
    return stream->mode;
}

void __fpurge(FILE* stream)
{
    fpurge(stream);
}
