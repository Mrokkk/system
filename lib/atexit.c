#include <stdlib.h>

#include "stdio/file.h"

typedef void (*handler_t)(void);

static size_t count;
static handler_t handlers[ATEXIT_MAX];

static void call_atexit_handlers(void)
{
    for (size_t i = count; count; count--)
    {
        handlers[i - 1]();
    }
}

int LIBC(atexit)(void (*function)(void))
{
    if (count == ATEXIT_MAX)
    {
        return -1;
    }
    handlers[count++] = function;
    return 0;
}

[[noreturn]] void LIBC(exit)(int error_code);

[[noreturn]] void __exit(int error_code)
{
    call_atexit_handlers();
    file_flush_all();
    while (1)
    {
        LIBC(exit)(error_code);
    }
}

extern void exit(int error_code) __attribute__((noreturn, alias("__exit")));

[[noreturn]] void ___exit(int error_code)
{
    while (1)
    {
        LIBC(exit)(error_code);
    }
}

extern void _Exit(int error_code) __attribute__((noreturn, weak, alias("___exit")));
extern void _exit(int error_code) __attribute__((noreturn, weak, alias("___exit")));

LIBC_ALIAS(atexit);
