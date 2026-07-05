#include <stdio.h>

int LIBC(scanf)(const char* format, ...)
{
    NOT_IMPLEMENTED(EOF, "\"%s\"", format);
}

int LIBC(fscanf)(FILE* stream, const char* format, ...)
{
    NOT_IMPLEMENTED(EOF, "%p, \"%s\"", stream, format);
}

int LIBC(vscanf)(const char* format, va_list ap)
{
    NOT_IMPLEMENTED(EOF, "\"%s\", %p", format, ap);
}

int LIBC(sscanf)(const char* restrict str, const char* restrict format, ...)
{
    NOT_IMPLEMENTED(-1, "%p, %s", str, format);
}

int LIBC(vfscanf)(FILE* stream, const char* format, va_list ap)
{
    NOT_IMPLEMENTED(EOF, "%p, \"%s\", %p", stream, format, ap);
}

LIBC_ALIAS(scanf);
LIBC_ALIAS(fscanf);
LIBC_ALIAS(vscanf);
LIBC_ALIAS(sscanf);
LIBC_ALIAS(vfscanf);
