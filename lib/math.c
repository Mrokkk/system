#include <math.h>

double LIBC(pow)(double x, double y)
{
    return __builtin_pow(x, y);
}

double LIBC(modf)(double x, double* iptr)
{
    return __builtin_modf(x, iptr);
}

LIBC_ALIAS(pow);
LIBC_ALIAS(modf);
