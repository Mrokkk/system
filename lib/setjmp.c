#include <setjmp.h>

_Static_assert(sizeof(struct __jmp_buf) == 32);

__attribute__((naked)) void longjmp(jmp_buf, int)
{
    asm volatile(
        "mov 4(%esp), %ecx;"
        "mov 4(%ecx), %ebx;"
        "mov 8(%ecx), %esi;"
        "mov 12(%ecx), %edi;"
        "mov 20(%ecx), %eax;"
        "push %eax;"
        "popfl;"
        "mov 16(%ecx), %edx;"
        "mov 8(%esp), %eax;"
        "mov 24(%ecx), %ebp;"
        "mov 28(%ecx), %esp;"
        "movl 0(%ecx), %edx;"
        "mov %edx, (%esp);"
        "jmp *%edx");
}

__attribute__((naked)) int setjmp(jmp_buf)
{
    asm volatile(
        "mov 4(%esp), %ecx;"
        "mov 0(%esp), %eax;"
        "pushfl;"
        "mov %eax, 0(%ecx);"
        "mov %ebx, 4(%ecx);"
        "mov %esi, 8(%ecx);"
        "mov %edi, 12(%ecx);"
        "movl $1f, 16(%ecx);"
        "pop %eax;"
        "mov %eax, 20(%ecx);"
        "mov %ebp, 24(%ecx);"
        "mov %esp, 28(%ecx);"
        "xor %eax, %eax;"
        "1:"
        "ret");
}

// FIXME: implement those properly
__attribute__((naked)) void siglongjmp(sigjmp_buf, int)
{
    asm volatile("jmp longjmp;");
}

__attribute__((naked)) int sigsetjmp(sigjmp_buf, int)
{
    asm volatile("jmp setjmp;");
}
