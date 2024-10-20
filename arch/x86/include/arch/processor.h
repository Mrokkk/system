#pragma once

#define CACHELINE_SIZE 64
#define IO_BITMAP_SIZE 128
#define IOMAP_OFFSET   104

#ifdef __i386__
#include <arch/processor_32.h>
#else
#include <arch/processor_64.h>
#endif

#ifndef __ASSEMBLER__

#include <stdint.h>

typedef struct { uint16_t off, seg; } farptr_t;

#define farptr(v) ptr((v).seg * 0x10 + (v).off)

#define CACHELINE_ALIGN ALIGN(CACHELINE_SIZE)

typedef struct signal_frame signal_frame_t;
typedef struct signal_context signal_context_t;

struct signal_frame
{
    int             sig;
    pt_regs_t*      context;
    signal_frame_t* prev;
};

#endif
