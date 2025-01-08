#include <stdio.h>

#include "file.h"

int LIBC(fclose)(FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream), EOF);
    return file_close(stream);
}

int LIBC(pclose)(FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream), EOF);
    return file_close(stream);
}

LIBC_ALIAS(fclose);
LIBC_ALIAS(pclose);
