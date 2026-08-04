#pragma once

#include <sys/cdefs.h>
#include <kernel/api/syscall.h>

__BEGIN_DECLS

int syscall(int nr, ...);
int vsyscall(int nr, ...);

__END_DECLS
