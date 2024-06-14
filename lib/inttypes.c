#include "libc.h"
#include <inttypes.h>

intmax_t LIBC(strtoimax)(const char* restrict nptr, char** restrict endptr, int base)
{
    NOT_IMPLEMENTED(0, "\"%s\", %p, %d", nptr, endptr, base);
}

uintmax_t LIBC(strtoumax)(const char* restrict nptr, char** restrict endptr, int base)
{
    NOT_IMPLEMENTED(0, "\"%s\", %p, %d", nptr, endptr, base);
}

LIBC_ALIAS(strtoimax);
LIBC_ALIAS(strtoumax);
