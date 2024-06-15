#include <stdio.h>
#include <unistd.h>

#include "file.h"

size_t LIBC(fwrite)(const void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream) && ptr, 0);

    for (size_t i = 0; i < size * nmemb; ++i)
    {
        stream->buffer.putc(&stream->buffer, ((char*)ptr)[i]);
    }

    return nmemb;
}

LIBC_ALIAS(fwrite);
