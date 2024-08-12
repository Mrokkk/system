#include <common/defs.h>

size_t NAME(strnlen)(const char* s, size_t maxlen)
{
    size_t res, temp1, temp2;

    asm volatile(
        "jmp 2f;"
        "1:"
        "cmpb $0, (%[s]);"
        "je 3f;"
        "incl %[s];"
        "2:"
        "decl %[maxlen];"
        "cmpl $-1, %[maxlen];"
        "jne 1b;"
        "3:"
        "subl %[start], %[s]"
        : "=a" (res), "=S" (temp1), "=D" (temp2)
        : [s] "a" (s), [start] "c" (s), [maxlen] "d" (maxlen)
        : "memory");

    return res;
}

POST(strnlen);
