#include <stdlib.h>

void* LIBC(bsearch)(
    const void* key,
    const void* base,
    size_t nmemb,
    size_t size,
    int (*compar)(const void*, const void*))
{
    for (size_t i = 0; i < nmemb; ++i)
    {
        if (!compar(key, base + i * size))
        {
            return (void*)base + i * size;
        }
    }
    return NULL;
}

LIBC_ALIAS(bsearch);
