#include <common/defs.h>

char* NAME(strrchr)(const char* s, int c)
{
    char* res;
    int temp;

    size_t len = NAME(strlen)(s);

    asm volatile(
        "std;"
        "movb %%al, %%ah;"
        "1:"
        "lodsb;"
        "cmp %%ah, %%al;"
        "je 2f;"
        "cmp %[start], %[s];"
        "jge 1b;"
        "movl $-1, %[s];"
        "2:"
        "movl %[s], %[res];"
        "inc %[res];"
        "cld;"
        : [res] "=a" (res), "=S" (temp)
        : [s] "S" (s + len), "a" (c), [start] "g" (s)
        : "memory");

    return res;
}

POST(strrchr);
