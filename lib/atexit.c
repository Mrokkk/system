#include <stdlib.h>

int LIBC(atexit)(void (*function)(void))
{
    NOT_IMPLEMENTED(0, "%p", function);
}

LIBC_ALIAS(atexit);
