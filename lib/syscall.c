#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/poll.h>

int syscall(int nr, ...)
{
    int res;
    va_list args;

    va_start(args, nr);
    u32 a1 = va_arg(args, u32);
    u32 a2 = va_arg(args, u32);
    u32 a3 = va_arg(args, u32);
    u32 a4 = va_arg(args, u32);
    u32 a5 = va_arg(args, u32);
    va_end(args);

    asm volatile(
        "int $0x80"
        : "=a" (res)
        : "a" (nr),
          "b" (a1),
          "c" (a2),
          "d" (a3),
          "S" (a4),
          "D" (a5)
        : "memory");

    if (UNLIKELY(res < 0 && res >= -ERRNO_MAX))
    {
        errno = -res;
        res = -1;
    }

    return res;
}

#define __syscall0(name, ret) \
    ret LIBC(name)(void) \
    { \
        return (ret)syscall(__NR_##name); \
    } \
    LIBC_ALIAS(name);

#define __syscall1(name, ret, t1) \
    ret LIBC(name)(typeof(t1) a1) \
    { \
        return (ret)syscall(__NR_##name, a1); \
    } \
    LIBC_ALIAS(name);

#define __syscall1_noret(name, ret, t1) \
    ret LIBC(name)(typeof(t1) a1) \
    { \
        syscall(__NR_##name, a1); \
        __builtin_unreachable(); \
    } \
    LIBC_ALIAS(name);

#define __syscall2(name, ret, t1, t2) \
    ret LIBC(name)(typeof(t1) a1, typeof(t2) a2) \
    { \
        return (ret)syscall(__NR_##name, a1, a2); \
    } \
    LIBC_ALIAS(name);

#define __syscall3(name, ret, t1, t2, t3) \
    ret LIBC(name)(typeof(t1) a1, typeof(t2) a2, typeof(t3) a3) \
    { \
        return (ret)syscall(__NR_##name, a1, a2, a3); \
    } \
    LIBC_ALIAS(name);

#define __syscall4(name, ret, t1, t2, t3, t4) \
    ret LIBC(name)(typeof(t1) a1, typeof(t2) a2, typeof(t3) a3, typeof(t4) a4) \
    { \
        return (ret)syscall(__NR_##name, a1, a2, a3, a4); \
    } \
    LIBC_ALIAS(name);

#define __syscall5(name, ret, t1, t2, t3, t4, t5) \
    ret LIBC(name)(typeof(t1) a1, typeof(t2) a2, typeof(t3) a3, typeof(t4) a4, typeof(t5) a5) \
    { \
        return (ret)syscall(__NR_##name, a1, a2, a3, a4, a5); \
    } \
    LIBC_ALIAS(name);

#define __syscall6(name, ret, t1, t2, t3, t4, t5, t6) \
    ret LIBC(name)(typeof(t1) a1, typeof(t2), typeof(t3), typeof(t4), typeof(t5), typeof(t6)) \
    { \
        return (ret)syscall(__NR_##name, &a1); \
    } \
    LIBC_ALIAS(name);

#include <kernel/api/syscall.h>

WEAK_ALIAS(exit, _exit);
WEAK_ALIAS(exit, _Exit);
