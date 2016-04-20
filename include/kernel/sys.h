#ifndef INCLUDE_KERNEL_SYS_H_
#define INCLUDE_KERNEL_SYS_H_

#define __syscall0(call) int sys_##call();
#define __syscall1(call, ...) int sys_##call();
#define __syscall2(call, ...) int sys_##call();
#define __syscall3(call, ...) int sys_##call();
#define __syscall4(call, ...) int sys_##call();
#include <kernel/unistd.h>

#endif /* INCLUDE_KERNEL_SYS_H_ */
