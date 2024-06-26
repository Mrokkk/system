#include <signal.h>
#include <stdlib.h>

[[noreturn]] void LIBC(abort)(void)
{
    if (raise(SIGABRT))
    {
        // If raise failed for some reason, just crash
        int* v = (int*)0;
        *v = 20;
    }

    __builtin_unreachable();
}

LIBC_ALIAS(abort);
