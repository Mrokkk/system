#include <stdlib.h>

double LIBC(strtod)(const char* nptr, char** endptr)
{
    return (double)strtol(nptr, endptr, 0);
}

float LIBC(strtof)(const char* nptr, char** endptr)
{
    return (float)strtol(nptr, endptr, 0);
}

long double LIBC(strtold)(const char* nptr, char** endptr)
{
    return (long double)strtol(nptr, endptr, 0);
}

LIBC_ALIAS(strtod);
LIBC_ALIAS(strtof);
LIBC_ALIAS(strtold);
