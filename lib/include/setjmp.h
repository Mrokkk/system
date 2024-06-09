#pragma once

#include <stdint.h>

struct __jmp_buf
{
    uint32_t ebx;       // 0
    uint32_t esi;       // 4
    uint32_t edi;       // 8
    uint32_t eip;       // 12
    uint32_t ebp;       // 16
    uint32_t esp;       // 20
};

typedef struct __jmp_buf jmp_buf[1];
typedef struct __jmp_buf sigjmp_buf[1];

void   longjmp(jmp_buf, int);
int    setjmp(jmp_buf);

void   siglongjmp(sigjmp_buf, int);
int    sigsetjmp(sigjmp_buf, int);
