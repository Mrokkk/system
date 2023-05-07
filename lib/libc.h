#pragma once

#define __LIBC

#ifndef __ASSEMBLER__

#define unlikely(x) __builtin_expect(!!(x), 0)

typedef unsigned char   u8;
typedef unsigned short  u16;
typedef unsigned long   u32;

#endif // __ASSEMBLER__
