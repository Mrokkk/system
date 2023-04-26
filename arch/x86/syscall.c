#include <kernel/kernel.h>

typedef int (*syscall_t)();

#define __syscall0(call, ...) int sys_##call();
#define __syscall1(call, ...) int sys_##call();
#define __syscall2(call, ...) int sys_##call();
#define __syscall3(call, ...) int sys_##call();
#define __syscall4(call, ...) int sys_##call();
#define __syscall5(call, ...) int sys_##call();
#define __syscall6(call, ...) int sys_##call();
#include <kernel/unistd.h>

#undef __syscall0
#undef __syscall1
#undef __syscall2
#undef __syscall3
#undef __syscall4
#undef __syscall5
#undef __syscall6
#define __syscall(call) [__NR_##call] = &sys_##call,
#define __syscall0(call, ...)   __syscall(call)
#define __syscall1(call, ...)   __syscall(call)
#define __syscall2(call, ...)   __syscall(call)
#define __syscall3(call, ...)   __syscall(call)
#define __syscall4(call, ...)   __syscall(call)
#define __syscall5(call, ...)   __syscall(call)
#define __syscall6(call, ...)   __syscall(call)

syscall_t syscalls[] = {
#include <kernel/unistd.h>
};
