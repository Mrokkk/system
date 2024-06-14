#include <unistd.h>

unsigned int LIBC(sleep)(unsigned int)
{
    return 0;
}

LIBC_ALIAS(sleep);
