#include <common/defs.h>

int NAME(strncmp)(const char* s1, const char* s2, size_t count)
{
    int res, temp1, temp2;

    asm volatile(
        "1:"
        "test %[count], %[count];"
        "jz 2f;"
        "decl %[count];"
        "lodsb; scasb;"
        "jne 3f;"
        "testb %%al, %%al;"
        "jne 1b;"
        "2:"
        "xorl %%eax, %%eax;"
        "jmp 4f;"
        "3:"
        "xor %%ecx, %%ecx;"
        "and $0xff, %%eax;"
        "mov -1(%%edi), %%cl;"
        "sub %%ecx, %%eax;"
        "4:"
        : "=a" (res), "=S" (temp1), "=D" (temp2)
        : "S" (s1), "D" (s2), [count] "c" (count)
        : "memory");

    return res;
}

POST(strncmp);
