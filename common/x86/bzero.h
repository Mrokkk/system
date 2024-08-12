#include <stddef.h>

static inline void bzero_impl(void* s, size_t count)
{
    int temp;

    asm volatile(
        "rep; stosl;"
        "mov %[count], %%ecx;"
        "and $3, %%ecx;"
        "jz 1f;"
        "rep; stosb;"
        "1:"
        : "=D" (temp)
        : "a" (0), "D" (s), "c" (count / 4), [count] "g" (count)
        : "memory");
}
