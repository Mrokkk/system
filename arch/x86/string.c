#include <arch/string.h>

// Below functions are copied from Linux source

#ifdef __HAVE_ARCH_MEMCPY
void* memcpy(void* to, const void* from, size_t n)
{
    int d0, d1, d2;
    asm volatile(
        "rep;"
        "movsl;"
        "movl %4, %%ecx;"
        "andl $3, %%ecx;"
        "jz 1f;"
        "rep;"
        "movsb;"
        "1:"
        : "=&c" (d0), "=&D" (d1), "=&S" (d2)
        : "0" (n / 4), "g" (n), "1" ((long)to), "2" ((long)from)
        : "memory");
    return to;
}
#endif

#ifdef __HAVE_ARCH_MEMSET
void* memset(void* s, int c, size_t count)
{
    int d0, d1;
    asm volatile(
        "rep;"
        "stosb;"
        : "=&c" (d0), "=&D" (d1)
        : "a" (c), "1" (s), "0" (count)
        : "memory");
    return s;
}
#endif
