#include <math.h>

double LIBC(log10)(double x)
{
    return __builtin_log10(x);
}

double LIBC(pow)(double x, double y)
{
    return __builtin_pow(x, y);
}

double LIBC(modf)(double x, double* iptr)
{
    return __builtin_modf(x, iptr);
}

LIBC_ALIAS(log10);
LIBC_ALIAS(pow);
LIBC_ALIAS(modf);
