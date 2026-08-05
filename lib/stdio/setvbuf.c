#include <stdio.h>

#include "file.h"

int LIBC(setvbuf)(FILE* stream, char* buf, int mode, size_t size)
{
    VALIDATE_INPUT(FILE_CHECK(stream), -1);
    return file_setbuf(stream, mode, buf, size);
}

void LIBC(setbuf)(FILE* restrict stream, char* buf)
{
    setvbuf(stream, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

void LIBC(setbuffer)(FILE* restrict stream, char* buf, size_t size)
{
    setvbuf(stream, buf, buf ? _IOFBF : _IONBF, size);
}

void LIBC(setlinebuf)(FILE* stream)
{
    setvbuf(stream, NULL, _IOLBF, BUFSIZ);
}

LIBC_ALIAS(setvbuf);
LIBC_ALIAS(setbuf);
LIBC_ALIAS(setbuffer);
LIBC_ALIAS(setlinebuf);
