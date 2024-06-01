#include <string.h>

void* memcpy(void* dest, const void* src, size_t size)
{
    size_t size4;
    uint32_t* d4;
    uint32_t* s4;
    uint8_t* d1;
    uint8_t* s1;

    for (size4 = size >> 2, d4 = (uint32_t*)dest, s4 = (uint32_t*)src;
         size4>0;
         size4--, *d4++ = *s4++);

    for (size = size % 4, d1 = (uint8_t*)d4, s1 = (uint8_t*)s4;
         size>0;
         size--, *d1++ = *s1++);

    return dest;
}
