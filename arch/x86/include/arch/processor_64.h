#pragma once

#ifndef __ASSEMBLER__

#include <stdint.h>
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
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint16_t ds, __ds[3];
    uint16_t es, __es[3];
    uint16_t fs, __fs[3];
    uint16_t gs, __gs[3];
    uint64_t rax;
    uint64_t error_code;
    uint64_t rip;
    uint16_t cs, __cs[3];
    uint32_t eflags, __eflags;
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

#endif // __ASSEMBLER__

#define REGS_RAX    144
#define REGS_CS     168
#define REGS_RSP    184

#define CONTEXT_RSP2 24

#define INIT_PROCESS_STACK_SIZE 1024 // number of uint32_t's

#define INIT_PROCESS_CONTEXT(name) \
    { \
        .rsp = (uintptr_t)&name##_stack[INIT_PROCESS_STACK_SIZE], \
    }

#define NEED_RESCHED_SIGNAL_OFFSET  40
