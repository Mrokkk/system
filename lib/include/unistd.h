#pragma once

#include <kernel/unistd.h>

#ifndef __ASSEMBLER__

int syscall(int nr, ...);

#endif // __ASSEMBLER__
