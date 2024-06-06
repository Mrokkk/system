#pragma once

#include <stdint.h>

struct __jmp_buf
{
    uint32_t ret;       // 0
    uint32_t ebx;       // 4
    uint32_t esi;       // 8
    uint32_t edi;       // 12
    uint32_t eip;       // 16
    uint32_t eflags;    // 20
    uint32_t ebp;       // 24
    uint32_t esp;       // 28
};

typedef struct __jmp_buf jmp_buf[1];
typedef struct __jmp_buf sigjmp_buf[1];

void   longjmp(jmp_buf, int);
int    setjmp(jmp_buf);

void   siglongjmp(sigjmp_buf, int);
int    sigsetjmp(sigjmp_buf, int);
