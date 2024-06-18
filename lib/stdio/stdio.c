#include <stdio.h>

#include "file.h"

FILE* stdin;
FILE* stdout;
FILE* stderr;

#define INIT_MUST_SUCCEED(x, data) \
    do \
    { \
        int __ret = x; \
        if (UNLIKELY(__ret)) \
        { \
            asm volatile( \
                "call 0xdeadbeef" \
                :: "a" (__ret), \
                   "b" (__LINE__), \
                   "c" (data)); \
        } \
    } \
    while (0)

void stdio_init(void)
{
    INIT_MUST_SUCCEED(file_allocate(STDIN_FILENO, O_RDONLY, &stdin), stdin);
    INIT_MUST_SUCCEED(file_allocate(STDOUT_FILENO, O_WRONLY, &stdout), stdout);
    INIT_MUST_SUCCEED(file_allocate(STDERR_FILENO, O_WRONLY, &stderr), stderr);
}

// Temporary
int LIBC(fsync)(int fd)
{
    NOT_IMPLEMENTED(0, "%d", fd);
}

LIBC_ALIAS(fsync);
