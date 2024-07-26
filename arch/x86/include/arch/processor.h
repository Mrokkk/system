#pragma once

#ifndef __ASSEMBLER__

#include <stdint.h>
#include <arch/segment.h>
#include <kernel/compiler.h>

#define CACHELINE_SIZE 64
#define IO_BITMAP_SIZE 128
#define IOMAP_OFFSET   104

struct tss
{
   uint32_t prev_tss;
   uint32_t esp0;
   uint32_t ss0;
   uint32_t esp1;
   uint32_t ss1;
   uint32_t esp2;
   uint32_t ss2;
   uint32_t cr3;
   uint32_t eip;
   uint32_t eflags;
   uint32_t eax;
   uint32_t ecx;
   uint32_t edx;
   uint32_t ebx;
   uint32_t esp;
   uint32_t ebp;
   uint32_t esi;
   uint32_t edi;
   uint32_t es;
   uint32_t cs;
   uint32_t ss;
   uint32_t ds;
   uint32_t fs;
   uint32_t gs;
   uint32_t ldt;
   uint16_t trap;
   uint16_t iomap_offset;
   uint8_t io_bitmap[IO_BITMAP_SIZE];
} PACKED;

struct context
{
    uint32_t eip;
    uint32_t esp;
    uint32_t esp0;
    uint32_t esp2;
};

typedef struct context context_t;
typedef struct tss tss_t;
extern tss_t tss;

struct pt_regs
{
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t eax;
    uint16_t ds, __ds;
    uint16_t es, __es;
    uint16_t fs, __fs;
    uint16_t gs, __gs;
    uint32_t error_code;
    uint32_t eip;
    uint16_t cs, __cs;
    uint32_t eflags;
    uint32_t esp;
    uint16_t ss, __ss;
} PACKED;

typedef struct pt_regs pt_regs_t;

struct context_switch_frame
{
    uint16_t gs, __gs;
    uint32_t ebp;
    uint32_t edi;
    uint32_t esi;
    uint32_t ecx;
    uint32_t ebx;
} PACKED;

typedef struct context_switch_frame context_switch_frame_t;
typedef struct signal_context signal_context_t;
typedef struct signal_frame signal_frame_t;

struct signal_frame
{
    int sig;
    pt_regs_t* context;
    signal_frame_t* prev;
};

struct signal_context
{
};

typedef struct { uint16_t off, seg; } farptr_t;

#define farptr(v) ptr((v).seg * 0x10 + (v).off)

#define CACHELINE_ALIGN ALIGN(CACHELINE_SIZE)

#endif // __ASSEMBLER__

#define REGS_EBX    0
#define REGS_ECX    4
#define REGS_EDX    8
#define REGS_ESI    12
#define REGS_EDI    16
#define REGS_EBP    20
#define REGS_EAX    24
#define REGS_DS     28
#define REGS_ES     32
#define REGS_FS     36
#define REGS_GS     40
#define REGS_EC     44
#define REGS_EIP    48
#define REGS_CS     52
#define REGS_EFLAGS 56
#define REGS_ESP    60
#define REGS_SS     64

#define CONTEXT_ESP2 12

#define INIT_PROCESS_STACK_SIZE 1024 // number of uint32_t's

#define INIT_PROCESS_CONTEXT(name) \
    { \
        .esp = (uint32_t)&name##_stack[INIT_PROCESS_STACK_SIZE], \
    }

#define NEED_RESCHED_SIGNAL_OFFSET  16
