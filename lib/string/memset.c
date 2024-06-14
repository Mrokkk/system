#include <string.h>

void* LIBC(memset)(void* ptr, int c, size_t size)
{
    for (size_t i = 0; i < size; ((char*)ptr)[i++] = c);
    return ptr;
}

LIBC_ALIAS(memset);
