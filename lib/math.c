#include <math.h>

#define BUILTIN1(name, type, param) \
    type LIBC(name)(type param) \
    { \
        return __builtin_##name(param); \
    } \
    LIBC_ALIAS(name);

BUILTIN1(ceil, double, x);
BUILTIN1(floor, double, x);
BUILTIN1(exp, double, x);
BUILTIN1(log, double, x);
BUILTIN1(log10, double, x);
BUILTIN1(sqrt, double, x);
BUILTIN1(sin, double, x);
BUILTIN1(sinh, double, x);
BUILTIN1(cos, double, x);
BUILTIN1(cosh, double, x);
BUILTIN1(tan, double, x);
BUILTIN1(tanh, double, x);
BUILTIN1(asin, double, x);
BUILTIN1(acos, double, x);
BUILTIN1(atan, double, x);

double LIBC(ldexp)(double x, int exp)
{
    return __builtin_ldexp(x, exp);
}
LIBC_ALIAS(ldexp);

double LIBC(atan2)(double y, double x)
{
    return __builtin_atan2(y, x);
}
LIBC_ALIAS(atan2);

double LIBC(pow)(double x, double y)
{
    return __builtin_pow(x, y);
}
LIBC_ALIAS(pow);

double LIBC(modf)(double x, double* iptr)
{
    return __builtin_modf(x, iptr);
}
LIBC_ALIAS(modf);

double LIBC(fmod)(double x, double y)
{
    return __builtin_atan2(y, x);
}
LIBC_ALIAS(fmod);
