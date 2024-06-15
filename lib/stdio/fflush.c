#include <stdio.h>

#include "file.h"

int LIBC(fflush)(FILE* stream)
{
    if (!stream)
    {
        file_flush_all();
        return 0;
    }

    if (stream->buffer.current == stream->buffer.start)
    {
        return 0;
    }

    return file_flush(stream);
}

LIBC_ALIAS(fflush);
