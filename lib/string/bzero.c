#include <string.h>
#include <strings.h>

void LIBC(explicit_bzero)(void* s, size_t n)
{
    bzero(s, n);
    asm volatile("" ::: "memory");
}

LIBC_ALIAS(explicit_bzero);
