#pragma once

#include <stdint.h>

struct __jmp_buf
{
    uint8_t data[32];
};

typedef struct __jmp_buf jmp_buf[1];
typedef struct __jmp_buf sigjmp_buf[1];

void   _longjmp(jmp_buf, int);
void   longjmp(jmp_buf, int);
void   siglongjmp(sigjmp_buf, int);

int    _setjmp(jmp_buf);
int    setjmp(jmp_buf);
int    sigsetjmp(sigjmp_buf, int);
