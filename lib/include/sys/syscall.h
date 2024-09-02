#pragma once

#include <kernel/api/syscall.h>

int syscall(int nr, ...);
int vsyscall(int nr, ...);
