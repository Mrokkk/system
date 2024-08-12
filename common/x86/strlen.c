#include <common/defs.h>

size_t NAME(strlen)(const char* s)
{
    int temp1, temp2;
    size_t res;

    asm volatile(
        "repne; scasb;"
        : "=c" (res), "=D" (temp1), "=A" (temp2)
        : "D" (s), "a" (0), "c" (0xffffffff)
        : "memory");

    return ~res - 1;
}

POST(strlen);
