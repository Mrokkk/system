#pragma once

#undef __syscall0
#undef __syscall1
#undef __syscall2
#undef __syscall3
#undef __syscall4
#define __syscall0(call) int sys_##call();
#define __syscall1(call, ...) int sys_##call();
#define __syscall2(call, ...) int sys_##call();
#define __syscall3(call, ...) int sys_##call();
#define __syscall4(call, ...) int sys_##call();
#include <kernel/unistd.h>
