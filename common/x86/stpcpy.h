#pragma once

static inline char* stpcpy_impl(char* dest, const char* src)
{
    char* res;
    int temp1, temp2;

    asm volatile(
        "1:"
        "lodsb; stosb;"
        "testb %%al, %%al;"
        "jne 1b;"
        : "=S" (temp1), "=D" (res), "=a" (temp2)
        : "S" (src), "D" (dest)
        : "memory");

    return res - 1;
}
