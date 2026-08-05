#include <stdlib.h>
#include <inttypes.h>
#include <common/div.h>

div_t LIBC(div)(int numerator, int denominator)
{
    int rem = DO_DIV(numerator, denominator);
    return (div_t){.quot = numerator, .rem = rem};
}

ldiv_t LIBC(ldiv)(long numerator, long denominator)
{
    long rem = DO_DIV(numerator, denominator);
    return (ldiv_t){.quot = numerator, .rem = rem};
}

lldiv_t LIBC(lldiv)(long long numerator, long long denominator)
{
    long long rem = DO_DIV(numerator, denominator);
    return (lldiv_t){.quot = numerator, .rem = rem};
}

imaxdiv_t LIBC(imaxdiv)(intmax_t numerator, intmax_t denominator)
{
    long long rem = DO_DIV(numerator, denominator);
    return (imaxdiv_t){.quot = numerator, .rem = rem};
}

LIBC_ALIAS(div);
LIBC_ALIAS(ldiv);
LIBC_ALIAS(lldiv);
LIBC_ALIAS(imaxdiv);
