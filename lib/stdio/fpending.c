#include "file.h"

size_t __fpending(FILE* stream)
{
    return stream->buffer.current - stream->buffer.start;
}
