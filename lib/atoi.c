#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

int LIBC(atoi)(const char* nptr)
{
    int digit;
    int value = 0;
    bool negative = false;

    for (; isspace(*nptr); ++nptr);

    if (*nptr == '-')
    {
        negative = true;
        ++nptr;
    }

    for (; *nptr && isdigit(*nptr); ++nptr)
    {
        digit = (int)*nptr - '0';
        value *= 10;
        value -= digit;
    }

    return negative ? value : -value;
}

LIBC_ALIAS(atoi);
