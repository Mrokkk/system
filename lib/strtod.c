#include <stdlib.h>

double LIBC(strtod)(const char* nptr, char** endptr)
{
    return (double)strtol(nptr, endptr, 0);
}

LIBC_ALIAS(strtod);
