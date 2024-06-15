#include <stdio.h>

#include "file.h"

int LIBC(fpurge)(FILE* stream)
{
    VALIDATE_INPUT(stream, -1);
    stream->buffer.current = stream->buffer.start;
    return 0;
}

LIBC_ALIAS(fpurge);
