#pragma once

#include <kernel/errno.h>

#if 0
#define errno ({ extern volatile int __errno; __errno; })
#endif

extern volatile int errno;

void perror(const char* s);
