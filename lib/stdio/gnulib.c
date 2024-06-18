#include <stdio.h>
#include "file.h"

// Functions required by gnulib (used by coreutils, grep, etc.)

const char* __freadptr(FILE* stream, size_t* sizep)
{
    *sizep = stream->buffer.current - stream->buffer.start;
    return *sizep ? stream->buffer.current : NULL;
}

size_t __freadahead(FILE* stream)
{
    size_t size = 0;;
    __freadptr(stream, &size);
    return size;
}

void __freadptrinc(FILE* stream, size_t increment)
{
    stream->buffer.current += increment;
    if (stream->buffer.current >= stream->buffer.end)
    {
        stream->buffer.current = stream->buffer.start;
    }
}

int __freadseek(FILE* stream, size_t offset)
{
    size_t pending;
    if ((pending = __freadahead(stream)))
    {
        int diff = pending - offset;
        if (diff > 0)
        {
            stream->buffer.current -= diff;
            return 0;
        }

        stream->buffer.current = stream->buffer.start;
        offset = -diff;
    }

    while (offset--)
    {
        fgetc(stream);
    }

    return 0;
}

void __fseterr(FILE* stream)
{
    stream->error = true;
}
