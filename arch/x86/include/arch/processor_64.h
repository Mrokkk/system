#pragma once

#ifndef __ASSEMBLER__

#include <stdint.h>
#include <arch/segment.h>
#include <kernel/compiler.h>

struct tss
{
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_offset;
    uint8_t  io_bitmap[IO_BITMAP_SIZE];
} PACKED;

struct context
{
    uint64_t rip;
    uint64_t rsp;
    uint64_t rsp0;
    uint64_t rsp2;
    uint64_t tls_base;
};

#define CONTEXT_IP(context) \
    ({ (context)->rip; })

#define CONTEXT_SP(context) \
    ({ (context)->rsp; })

#define CONTEXT_SP0(context) \
    ({ (context)->rsp0; })

#define CONTEXT_SP2(context) \
    ({ (context)->rsp2; })

typedef struct context context_t;
typedef struct tss tss_t;
extern tss_t tss;

struct pt_regs
{
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rax;
    uint16_t ds, __ds[3];
    uint16_t es, __es[3];
    uint16_t fs, __fs[3];
    uint16_t gs, __gs[3];
    uint64_t error_code;
    uint64_t rip;
    uint16_t cs, __cs[3];
    uint64_t eflags;
    uint64_t rsp;
    uint16_t ss, __ss[3];
} PACKED;

#define PT_REGS_IP(regs) \
    ({ (regs)->rip; })

#define PT_REGS_SP(regs) \
    ({ (regs)->rsp; })

#define PT_REGS_BP(regs) \
    ({ (regs)->rbp; })

typedef struct pt_regs pt_regs_t;

struct context_switch_frame
{
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rcx;
    uint64_t rbx;
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

#ifdef __i386__
#define INIT_PROCESS_CONTEXT(name) \
    { \
        .esp = (uintptr_t)&name##_stack[INIT_PROCESS_STACK_SIZE], \
    }
#else
#define INIT_PROCESS_CONTEXT(name) \
    { \
    }
#endif

#define NEED_RESCHED_SIGNAL_OFFSET  40
