#include <stdio.h>
#include <unistd.h>

#include "file.h"

size_t LIBC(fread)(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream) && ptr, 0);
    int count = read(stream->fd, ptr, size * nmemb);

    if (UNLIKELY(count < 0))
    {
        stream->error = true;
        return 0;
    }

    return count;
}

LIBC_ALIAS(fread);
