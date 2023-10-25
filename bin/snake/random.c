#include "random.h"

int last = 1032;

int random()
{
    int x = last;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return last = x;
}

void srand(unsigned int seed)
{
    last = (int)seed;
}
