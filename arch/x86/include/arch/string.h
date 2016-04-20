#ifndef __X86_STRING_H_
#define __X86_STRING_H_

#include <kernel/compiler.h>
#include <kernel/types.h>

/*===========================================================================*
 *                                   strlen                                  *
 *===========================================================================*/
#define __HAVE_ARCH_STRLEN
extern inline size_t strlen(const char *s) {
    char *temp;
    for (temp=(char *)s; *temp!=0; temp++);
    return temp-s;
}

/*===========================================================================*
 *                                  strnlen                                  *
 *===========================================================================*/
#define __HAVE_ARCH_STRNLEN
extern inline size_t strnlen(const char * s, size_t count) {
    int    d0;
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
extern inline void * __memcpy_by4(void * to, const void * from, size_t n) {
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
extern inline void * __memcpy_by2(void * to, const void * from, size_t n) {
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
extern inline void * __memcpy_g(void * to, const void * from, size_t n) {
    int    d0, d1, d2;
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


extern volatile char sse_enabled;

#define __memcpy(d, s, count) \
    (__builtin_constant_p(count) ? \
     __memcpy_c((d),(s),(count)) : \
     __memcpy_g((d),(s),(count)));

/* for small memory blocks (<256 bytes) this version is faster */
#define small_memcpy(to,from,n)\
{\
    register unsigned long int dummy;\
    __asm__ __volatile__(\
        "rep; movsb"\
        :"=&D"(to), "=&S"(from), "=&c"(dummy)\
        :"0" (to), "1" (from),"2" (n)\
        : "memory");\
}

#define SSE_MMREG_SIZE 16
#define MMX_MMREG_SIZE 8

#define MMX1_MIN_LEN 0x800  /* 2K blocks */
#define MIN_LEN 0x40  /* 64-byte blocks */

/* SSE note: i tried to move 128 bytes a time instead of 64 but it
didn't make any measureable difference. i'm using 64 for the sake of
simplicity. [MF] */
extern inline void *sse_memcpy(void * to, const void * from, size_t len) {

    void *retval;
    size_t i;
    retval = to;

    /* PREFETCH has effect even for MOVSB instruction ;) */
    __asm__ __volatile__ (
        "   prefetchnta (%0)\n"
        "   prefetchnta 32(%0)\n"
        "   prefetchnta 64(%0)\n"
        "   prefetchnta 96(%0)\n"
        "   prefetchnta 128(%0)\n"
        "   prefetchnta 160(%0)\n"
        "   prefetchnta 192(%0)\n"
        "   prefetchnta 224(%0)\n"
        "   prefetchnta 256(%0)\n"
        "   prefetchnta 288(%0)\n"
        : : "r" (from)
    );

    if(len >= MIN_LEN) {

        register unsigned long int delta;
        /* Align destinition to MMREG_SIZE -boundary */
        delta = ((unsigned long int)to)&(SSE_MMREG_SIZE-1);
        if(delta) {
            delta=SSE_MMREG_SIZE-delta;
            len -= delta;
            small_memcpy(to, from, delta);
        }
        i = len >> 6; /* len/64 */
        len&=63;
        if(((unsigned long)from) & 15)
        /* if SRC is misaligned */
            for(; i>0; i--) {
                __asm__ __volatile__ (
                    "prefetchnta 320(%0)\n"
                    "prefetchnta 352(%0)\n"
                    "movups (%0), %%xmm0\n"
                    "movups 16(%0), %%xmm1\n"
                    "movups 32(%0), %%xmm2\n"
                    "movups 48(%0), %%xmm3\n"
                    "movntps %%xmm0, (%1)\n"
                    "movntps %%xmm1, 16(%1)\n"
                    "movntps %%xmm2, 32(%1)\n"
                    "movntps %%xmm3, 48(%1)\n"
                    :: "r" (from), "r" (to) : "memory"
                );
                from = (const unsigned char *)((unsigned int)from + 64);
                to = (unsigned char *)((unsigned int)to + 64);
            }
        else
        /*
        Only if SRC is aligned on 16-byte boundary.
        It allows to use movaps instead of movups, which required data
        to be aligned or a general-protection exception (#GP) is generated.
        */
            for(; i>0; i--) {
                __asm__ __volatile__ (
                    "prefetchnta 320(%0)\n"
                    "prefetchnta 352(%0)\n"
                    "movaps (%0), %%xmm0\n"
                    "movaps 16(%0), %%xmm1\n"
                    "movaps 32(%0), %%xmm2\n"
                    "movaps 48(%0), %%xmm3\n"
                    "movntps %%xmm0, (%1)\n"
                    "movntps %%xmm1, 16(%1)\n"
                    "movntps %%xmm2, 32(%1)\n"
                    "movntps %%xmm3, 48(%1)\n"
                    :: "r" (from), "r" (to) : "memory"
                );
                from = (const unsigned char *)((unsigned int)from + 64);
                to = (unsigned char *)((unsigned int)to + 64);
            }
        /* since movntq is weakly-ordered, a "sfence"
        * is needed to become ordered again. */
        __asm__ __volatile__ ("sfence":::"memory");
        /* enables to use FPU */
        __asm__ __volatile__ ("emms":::"memory");
    }
    /*
    *    Now do the tail of the block
    */
    if(len) __memcpy(to, from, len);
    return retval;
}

#if 1
/*===========================================================================*
 *                                  memcpy                                   *
 *===========================================================================*/
#define __HAVE_ARCH_MEMCPY
extern inline void *memcpy(void *d, const void *s, size_t count) {
    if (sse_enabled == 1)
        return sse_memcpy(d, s, count);
    else
        return __memcpy(d, s, count);
}
#else
extern inline void *memcpy(void *d, const void *s, size_t count) {

    char *src = (char *)s;
    char *dest = (char *)d;

    if (src == dest || count == 0) return d;

    while (count--) *dest++ = *src++;

    return d;

}
#endif

#endif /* __X86_STRING_H_ */
