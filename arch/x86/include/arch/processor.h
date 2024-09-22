#pragma once

#define CACHELINE_SIZE 64
#define IO_BITMAP_SIZE 128
#define IOMAP_OFFSET   104

#ifndef __ASSEMBLER__

#include <stdint.h>

typedef struct { uint16_t off, seg; } farptr_t;

#define farptr(v) ptr((v).seg * 0x10 + (v).off)

#define CACHELINE_ALIGN ALIGN(CACHELINE_SIZE)

#endif

#ifdef __i386__
#include <arch/processor_32.h>
#else
#include <arch/processor_64.h>
#endif
