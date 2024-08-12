#pragma once

#include <stddef.h>

static inline void* mempcpy_impl(void* dest, const void* src, size_t n)
{
    void* res;

    asm volatile(
        "rep; movsl;"
        "movl %[size], %%ecx;"
        "andl $3, %%ecx;"
        "jz 1f;"
        "rep; movsb;"
        "1:"
        : "=D" (res)
        : "c" (n / 4), [size] "g" (n), "D" (dest), "S" (src)
        : "memory");

    return res;
}
