#include <stdio.h>

#include "file.h"

int LIBC(setvbuf)(FILE* stream, char* buf, int mode, size_t size)
{
    VALIDATE_INPUT(FILE_CHECK(stream), -1);
    return file_setbuf(stream, mode, buf, size);
}

LIBC_ALIAS(setvbuf);
