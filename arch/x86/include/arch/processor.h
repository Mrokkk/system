#pragma once

#ifndef __ASSEMBLER__

#include <stdint.h>
#include <arch/segment.h>
#include <kernel/compiler.h>

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
} __packed;

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
    uint32_t eip;
    uint16_t cs, __cs;
    uint32_t eflags;
    uint32_t esp;
    uint16_t ss, __ss;
} __packed;

#define __regs_print(regs, print_function) \
    do { \
        char buffer[64]; \
        print_function("eax = %08x", (regs)->eax); \
        print_function("ebx = %08x", (regs)->ebx); \
        print_function("ecx = %08x", (regs)->ecx); \
        print_function("edx = %08x", (regs)->edx); \
        print_function("esp = %04x:%08x", (regs)->ss, (regs)->esp); \
        print_function("ebp = %08x", (regs)->ebp); \
        print_function("eip = %04x:%08x", (regs)->cs, (regs)->eip); \
        print_function("ds = %04x; es = %04x; fs = %04x; gs = %04x", \
            (regs)->ds, \
            (regs)->es, \
            (regs)->fs, \
            (regs)->gs); \
        eflags_bits_string_get((regs)->eflags, buffer); \
        print_function("eflags = %08x = (iopl=%d %s)", \
            (regs)->eflags, \
            ((uint32_t)(regs)->eflags >> 12) & 0x3, \
            buffer); \
    } while (0)

#define PRINT_REGS_1(regs)                  __regs_print((regs), log_debug)
#define PRINT_REGS_2(regs, print_function)  __regs_print((regs), print_function)

#define regs_print(...) \
    REAL_VAR_MACRO_2(PRINT_REGS_1, PRINT_REGS_2, __VA_ARGS__)

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
#define REGS_EIP    44
#define REGS_CS     48
#define REGS_EFLAGS 52
#define REGS_ESP    56
#define REGS_SS     60

#define INIT_PROCESS_STACK_SIZE 512 // number of uint32_t's

#define INIT_PROCESS_CONTEXT(name) \
    { \
        .iomap_offset = IOMAP_OFFSET, \
        .esp = (uint32_t)&name##_stack[INIT_PROCESS_STACK_SIZE], \
        .ss0 = KERNEL_DS, \
    }
