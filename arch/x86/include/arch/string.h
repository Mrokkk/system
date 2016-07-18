#ifndef __X86_STRING_H_
#define __X86_STRING_H_

#include <kernel/compiler.h>
#include <kernel/types.h>

/* These functions are part of Linux source */

/*===========================================================================*
 *                                  strnlen                                  *
 *===========================================================================*/
#define __HAVE_ARCH_STRNLEN
static inline size_t strnlen(const char * s, size_t count) {
    int d0;
    register int __res;
    asm volatile(
        "movl %3, %0            \n"
        "jmp 2f                 \n"
        "1:                     \n"
        "    cmpb $0, (%0)      \n"
        "    je 3f              \n"
        "    incl %0            \n"
        "2:                     \n"
        "    decl %2            \n"
        "    cmpl $-1, %2       \n"
        "    jne 1b             \n"
        "3:                     \n"
        "    subl %3, %0        \n"
        :"=a" (__res), "=&d" (d0)
        :"1" (count), "c" (s));
    return __res;
}

/*===========================================================================*
 *                               __memcpy_by4                                *
 *===========================================================================*/
static inline void * __memcpy_by4(void * to, const void * from, size_t n) {
    register void *tmp = (void *)to;
    register int dummy1,dummy2;
    asm volatile(
        "1:                     \n"
        "    movl (%2), %0      \n"
        "    addl $4, %2        \n"
        "    movl %0, (%1)      \n"
        "    addl $4, %1        \n"
        "    decl %3            \n"
        "    jnz 1b             \n"
        : "=r" (dummy1), "=r" (tmp), "=r" (from), "=r" (dummy2)
        : "1" (tmp), "2" (from), "3" (n/4)
        : "memory");
    return (to);
}

/*===========================================================================*
 *                               __memcpy_by2                                *
 *===========================================================================*/
static inline void * __memcpy_by2(void * to, const void * from, size_t n) {
    register void *tmp = (void *)to;
    register int dummy1,dummy2;
    asm volatile(
        "shrl $1, %3            \n"
        "jz 2f                  \n"
        "1:                     \n"
        "    movl (%2), %0      \n"
        "    addl $4, %2        \n"
        "    movl %0, (%1)      \n"
        "    addl $4, %1        \n"
        "    decl %3            \n"
        "    jnz 1b             \n"
        "2:                     \n"
        "    movw (%2), %w0     \n"
        "    movw %w0, (%1)     \n"
        : "=r" (dummy1), "=r" (tmp), "=r" (from), "=r" (dummy2)
        : "1" (tmp), "2" (from), "3" (n/2)
        : "memory");
    return (to);
}

/*===========================================================================*
 *                                __memcpy_g                                 *
 *===========================================================================*/
static inline void * __memcpy_g(void * to, const void * from, size_t n) {
    int d0, d1, d2;
    register void *tmp = (void *)to;
    asm volatile(
        "shrl $1, %%ecx         \n"
        "jnc 1f                 \n"
        "movsb                  \n"
        "1:                     \n"
        "    shrl $1, %%ecx     \n"
        "    jnc 2f             \n"
        "    movsw              \n"
        "2:                     \n"
        "    rep                \n"
        "    movsl              \n"
        : "=&c" (d0), "=&D" (d1), "=&S" (d2)
        : "0" (n), "1" ((long) tmp), "2" ((long) from)
        : "memory");
    return (to);
}

/*===========================================================================*
 *                                __memcpy_c                                 *
 *===========================================================================*/
#define __memcpy_c(d,s,count) \
((count%4==0) ? \
 __memcpy_by4((d),(s),(count)) : \
 ((count%2==0) ? \
  __memcpy_by2((d),(s),(count)) : \
  __memcpy_g((d),(s),(count))))

#define __memcpy(d, s, count) \
    (__builtin_constant_p(count) ? \
     __memcpy_c((d),(s),(count)) : \
     __memcpy_g((d),(s),(count)));

/*===========================================================================*
 *                                  memcpy                                   *
 *===========================================================================*/
#define __HAVE_ARCH_MEMCPY
static inline void *memcpy(void *d, const void *s, size_t count) {

    return __memcpy(d, s, count);

}

#endif /* __X86_STRING_H_ */
