#include <stdlib.h>

int LIBC(atexit)(void (*function)(void))
{
    NOT_IMPLEMENTED(-1, "%p", function);
}

LIBC_ALIAS(atexit);
