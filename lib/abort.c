#include <signal.h>
#include <stdlib.h>

[[noreturn]] void LIBC(abort)(void)
{
    if (raise(SIGABRT))
    {
        // If raise failed for some reason, just crash
        __builtin_trap();
    }

    __builtin_unreachable();
}

LIBC_ALIAS(abort);
