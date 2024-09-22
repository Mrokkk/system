#include <common/defs.h>

#ifdef __i386__
int NAME(strcmp)(const char* s1, const char* s2)
{
    int res, temp1, temp2;

    asm volatile(
        "1:"
        "lodsb; scasb;"
        "jne 2f;"
        "test %%al, %%al;"
        "jne 1b;"
        "xor %%eax, %%eax;"
        "jmp 3f;"
        "2:"
        "and $0xff, %[res];"
        "mov -1(%[s2]), %%cl;"
        "sub %%ecx, %[res];"
        "3:"
        : [res] "=a" (res), "=D" (temp1), "=S" (temp2)
        : [s1] "S" (s1), [s2] "D" (s2), "c" (0)
        : "memory");

    return res;
}

POST(strcmp);
#endif
