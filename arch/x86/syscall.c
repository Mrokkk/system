#include <kernel/kernel.h>

typedef int (*syscall_t)();

#define __syscall(call, ...)    int sys_##call();
#define __syscall0(...)         __syscall(__VA_ARGS__)
#define __syscall1(...)         __syscall(__VA_ARGS__)
#define __syscall1_noret(...)   __syscall(__VA_ARGS__)
#define __syscall2(...)         __syscall(__VA_ARGS__)
#define __syscall3(...)         __syscall(__VA_ARGS__)
#define __syscall4(...)         __syscall(__VA_ARGS__)
#define __syscall5(...)         __syscall(__VA_ARGS__)
#define __syscall6(...)         __syscall(__VA_ARGS__)
#include <kernel/unistd.h>

#undef __syscall
#undef __syscall0
#undef __syscall1
#undef __syscall1_noret
#undef __syscall2
#undef __syscall3
#undef __syscall4
#undef __syscall5
#undef __syscall6
#define __syscall(call, ...)    [__NR_##call] = &sys_##call,
#define __syscall0(...)         __syscall(__VA_ARGS__)
#define __syscall1(...)         __syscall(__VA_ARGS__)
#define __syscall1_noret(...)   __syscall(__VA_ARGS__)
#define __syscall2(...)         __syscall(__VA_ARGS__)
#define __syscall3(...)         __syscall(__VA_ARGS__)
#define __syscall4(...)         __syscall(__VA_ARGS__)
#define __syscall5(...)         __syscall(__VA_ARGS__)
#define __syscall6(...)         __syscall(__VA_ARGS__)

syscall_t syscalls[] = {
#include <kernel/unistd.h>
};
