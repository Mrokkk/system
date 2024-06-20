#pragma once

#ifndef __ASSEMBLER__

#include <stdint.h>
#include <arch/segment.h>
#include <kernel/compiler.h>

#define CACHELINE_SIZE 64
#define IO_BITMAP_SIZE 128
#define IOMAP_OFFSET 104

struct context
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

typedef struct context context_t;

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

struct signal_context
{
    uint32_t esp, esp0, esp2, eip;
};

typedef struct signal_context signal_context_t;

typedef struct { uint16_t off, seg; } farptr_t;

#define farptr(v) ptr((v).seg * 0x10 + (v).off)

#define __regs_print(regs, print_function, header) \
    do { \
        char buffer[64]; \
        print_function("%s: eax = %08x", header, (regs)->eax); \
        print_function("%s: ebx = %08x", header, (regs)->ebx); \
        print_function("%s: ecx = %08x", header, (regs)->ecx); \
        print_function("%s: edx = %08x", header, (regs)->edx); \
        print_function("%s: esi = %08x", header, (regs)->esi); \
        print_function("%s: edi = %08x", header, (regs)->edi); \
        print_function("%s: esp = %04x:%08x", header, (regs)->ss, (regs)->esp); \
        print_function("%s: ebp = %08x", header, (regs)->ebp); \
        print_function("%s: eip = %04x:%08x", header, (regs)->cs, (regs)->eip); \
        print_function("%s: ds = %04x; es = %04x; fs = %04x; gs = %04x", \
            header, \
            (regs)->ds, \
            (regs)->es, \
            (regs)->fs, \
            (regs)->gs); \
        eflags_bits_string_get((regs)->eflags, buffer); \
        print_function("%s: eflags = %08x = (iopl=%d %s)", \
            header, \
            (regs)->eflags, \
            ((uint32_t)(regs)->eflags >> 12) & 0x3, \
            buffer); \
    } while (0)

#define PRINT_REGS_1(header, regs)                  __regs_print((regs), log_debug, header)
#define PRINT_REGS_2(header, regs, print_function)  __regs_print((regs), print_function, header)

#define regs_print(...) \
    REAL_VAR_MACRO_3(PRINT_REGS_1, PRINT_REGS_1, PRINT_REGS_2, __VA_ARGS__)

#define CACHELINE_ALIGN __attribute__((aligned(CACHELINE_SIZE)))

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

#define CONTEXT_ESP2 20

#define INIT_PROCESS_STACK_SIZE 1024 // number of uint32_t's

#define INIT_PROCESS_CONTEXT(name) \
    { \
        .iomap_offset = IOMAP_OFFSET, \
        .esp = (uint32_t)&name##_stack[INIT_PROCESS_STACK_SIZE], \
        .ss0 = KERNEL_DS, \
    }
