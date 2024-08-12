#include <common/defs.h>

char* NAME(strchr)(const char* s, int c)
{
    int temp;
    char* res;

    asm volatile(
        "movb %%al, %%ah;"
        "1:"
        "lodsb;"
        "cmpb %%ah, %%al;"
        "je 2f;"
        "testb %%al, %%al;"
        "jne 1b;"
        "movl $1, %[s];"
        "2:"
        "movl %[s], %[res];"
        "decl %[res];"
        : [res] "=a" (res), "=S" (temp)
        : [s] "S" (s), "a" (c)
        : "memory");

    return res;
}

POST(strchr);
