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
    INIT_MUST_SUCCEED(file_allocate(STDIN_FILENO, &stdin), stdin);
    INIT_MUST_SUCCEED(file_allocate(STDOUT_FILENO, &stdout), stdout);
    INIT_MUST_SUCCEED(file_allocate(STDERR_FILENO, &stderr), stderr);
}
