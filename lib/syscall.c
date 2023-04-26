#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>

int syscall(int nr, ...)
{
    int res;
    va_list args;

    va_start(args, nr);
    unsigned long a1 = va_arg(args, unsigned long);
    unsigned long a2 = va_arg(args, unsigned long);
    unsigned long a3 = va_arg(args, unsigned long);
    unsigned long a4 = va_arg(args, unsigned long);
    unsigned long a5 = va_arg(args, unsigned long);
    va_end(args);

    asm volatile(
        "push %%ebx;"
        "push %%esi;"
        "push %%edi;"
        "mov %1, %%eax;"
        "mov %2, %%ebx;"
        "mov %3, %%ecx;"
        "mov %4, %%edx;"
        "mov %5, %%esi;"
        "mov %6, %%edi;"
        "int $0x80;"
        "mov %%eax, %0;"
        "pop %%edi;"
        "pop %%esi;"
        "pop %%ebx;"
        : "=m" (res)
        : "m" (nr),
          "m" (a1),
          "m" (a2),
          "m" (a3),
          "m" (a4),
          "m" (a5)
        : "memory");

    if (unlikely(res < 0 && res > -134))
    {
        errno = res;
        res = -1;
    }

    return res;
}

#define __syscall0(name, ret) \
    ret name(void) \
    { \
        return (ret)syscall(__NR_##name); \
    }

#define __syscall1(name, ret, t1) \
    ret name(typeof(t1) a1) \
    { \
        return (ret)syscall(__NR_##name, a1); \
    }

#define __syscall2(name, ret, t1, t2) \
    ret name(typeof(t1) a1, typeof(t2) a2) \
    { \
        return (ret)syscall(__NR_##name, a1, a2); \
    }

#define __syscall3(name, ret, t1, t2, t3) \
    ret name(typeof(t1) a1, typeof(t2) a2, typeof(t3) a3) \
    { \
        return (ret)syscall(__NR_##name, a1, a2, a3); \
    }

#define __syscall4(name, ret, t1, t2, t3, t4) \
    ret name(typeof(t1) a1, typeof(t2) a2, typeof(t3) a3, typeof(t4) a4) \
    { \
        return (ret)syscall(__NR_##name, a1, a2, a3, a4); \
    }

#define __syscall5(name, ret, t1, t2, t3, t4, t5) \
    ret name(typeof(t1) a1, typeof(t2) a2, typeof(t3) a3, typeof(t4) a4, typeof(t5) a5) \
    { \
        return (ret)syscall(__NR_##name, a1, a2, a3, a4, a5); \
    }

#define __syscall6(name, ret, t1, t2, t3, t4, t5, t6) \
    ret name(typeof(t1) a1, typeof(t2), typeof(t3), typeof(t4), typeof(t5), typeof(t6)) \
    { \
        return (ret)syscall(__NR_##name, &a1); \
    }

#include <kernel/unistd.h>
