#pragma once

#include <kernel/types.h>
#include <kernel/compiler.h>

// Below functions are copied from Linux source

#define __HAVE_ARCH_STRLEN
static inline size_t strlen(const char* s)
{
    int d0;
    size_t res;
    asm volatile(
        "repne;"
        "scasb;"
        : "=c" (res), "=&D" (d0)
        : "1" (s), "a" (0), "0" (0xffffffffu)
        : "memory");
    return ~res - 1;
}

#define __HAVE_ARCH_STRNLEN
static inline size_t strnlen(const char* s, size_t count)
{
    int d0;
    int res;
    asm volatile(
        "movl %2,%0\n\t"
        "jmp 2f\n"
        "1:\tcmpb $0,(%0)\n\t"
        "je 3f\n\t"
        "incl %0\n"
        "2:\tdecl %1\n\t"
        "cmpl $-1,%1\n\t"
        "jne 1b\n"
        "3:\tsubl %2,%0"
        : "=a" (res), "=&d" (d0)
        : "c" (s), "1" (count)
        : "memory");
    return res;
}

#define __HAVE_ARCH_STRCMP
static inline int strcmp(const char* cs, const char* ct)
{
    int d0, d1;
    int res;
    asm volatile(
        "1:\tlodsb\n\t"
        "scasb\n\t"
        "jne 2f\n\t"
        "testb %%al,%%al\n\t"
        "jne 1b\n\t"
        "xorl %%eax,%%eax\n\t"
        "jmp 3f\n"
        "2:\tsbbl %%eax,%%eax\n\t"
        "orb $1,%%al\n"
        "3:"
        : "=a" (res), "=&S" (d0), "=&D" (d1)
        : "1" (cs), "2" (ct)
        : "memory");
    return res;
}

#define __HAVE_ARCH_STRNCMP
static inline int strncmp(const char* cs, const char* ct, size_t count)
{
    int res;
    int d0, d1, d2;
    asm volatile("1:\tdecl %3\n\t"
        "js 2f\n\t"
        "lodsb\n\t"
        "scasb\n\t"
        "jne 3f\n\t"
        "testb %%al,%%al\n\t"
        "jne 1b\n"
        "2:\txorl %%eax,%%eax\n\t"
        "jmp 4f\n"
        "3:\tsbbl %%eax,%%eax\n\t"
        "orb $1,%%al\n"
        "4:"
        : "=a" (res), "=&S" (d0), "=&D" (d1), "=&c" (d2)
        : "1" (cs), "2" (ct), "3" (count)
        : "memory");
    return res;
}

#define __HAVE_ARCH_STRCHR
static inline char* strchr(const char* s, int c)
{
    int d0;
    char *res;
    asm volatile("movb %%al,%%ah\n"
        "1:\tlodsb\n\t"
        "cmpb %%ah,%%al\n\t"
        "je 2f\n\t"
        "testb %%al,%%al\n\t"
        "jne 1b\n\t"
        "movl $1,%1\n"
        "2:\tmovl %1,%0\n\t"
        "decl %0"
        : "=a" (res), "=&S" (d0)
        : "1" (s), "0" (c)
        : "memory");
    return res;
}

#define __HAVE_ARCH_MEMCPY
#define memcpy __builtin_memcpy

#define __HAVE_ARCH_MEMSET
#define memset __builtin_memset
