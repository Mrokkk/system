#include <setjmp.h>

_Static_assert(sizeof(struct __jmp_buf) == 24);

#define LONGJMP() \
    asm volatile( \
        "mov 4(%esp), %ecx;" \
        "mov 0(%ecx), %ebx;" \
        "mov 4(%ecx), %esi;" \
        "mov 8(%ecx), %edi;" \
        "mov 12(%ecx), %edx;" \
        "mov 8(%esp), %eax;" \
        "mov 16(%ecx), %ebp;" \
        "mov 20(%ecx), %esp;" \
        "jmp *%edx")

#define SETJMP() \
    asm volatile( \
        "mov 4(%esp), %ecx;" \
        "mov %ebx, 0(%ecx);" \
        "mov %esi, 4(%ecx);" \
        "mov %edi, 8(%ecx);" \
        "mov (%esp), %eax;" \
        "movl %eax, 12(%ecx);" \
        "mov %ebp, 16(%ecx);" \
        "mov %esp, 20(%ecx);" \
        "xor %eax, %eax;" \
        "ret")

__attribute__((naked)) void LIBC(longjmp)(jmp_buf, int)
{
    LONGJMP();
}

__attribute__((naked)) int LIBC(setjmp)(jmp_buf)
{
    SETJMP();
}

__attribute__((naked)) void LIBC(siglongjmp)(jmp_buf, int)
{
    LONGJMP();
}

__attribute__((naked)) int LIBC(sigsetjmp)(jmp_buf, int)
{
    SETJMP();
}

LIBC_ALIAS(longjmp);
LIBC_ALIAS(setjmp);
LIBC_ALIAS(sigsetjmp);
LIBC_ALIAS(siglongjmp);
