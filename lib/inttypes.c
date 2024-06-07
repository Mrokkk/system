#include "libc.h"
#include <inttypes.h>

intmax_t LIBC(strtoimax)(const char* restrict nptr, char** restrict endptr, int base)
{
    UNUSED(nptr); UNUSED(endptr); UNUSED(base);
    return 0;
}
WEAK_ALIAS(LIBC(strtoimax), strtoimax);

uintmax_t LIBC(strtoumax)(const char* restrict nptr, char** restrict endptr, int base)
{
    UNUSED(nptr); UNUSED(endptr); UNUSED(base);
    return 0;
}
WEAK_ALIAS(LIBC(strtoumax), strtoumax);
