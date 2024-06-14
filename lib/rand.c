#include <stdlib.h>

int last = 1032;

int LIBC(rand)(void)
{
    int x = last;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return last = x;
}

void LIBC(srand)(unsigned int seed)
{
    last = (int)seed;
}

LIBC_ALIAS(rand);
LIBC_ALIAS(srand);
