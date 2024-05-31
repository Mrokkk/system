#include <stdlib.h>

int atexit(void (*function)(void))
{
    UNUSED(function);
    return -1;
}
