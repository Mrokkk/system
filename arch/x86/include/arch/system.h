#pragma once

#include <stdint.h>
#include <arch/segment.h>
#include <kernel/compiler.h>

typedef uint32_t flags_t;

#define mb() \
    asm volatile("" : : : "memory")

#define flags_save(x) \
    asm volatile("pushfl; popl %0" : "=r" (x) : /*  */ : "memory")

#define flags_restore(x) \
    asm volatile("pushl %0; popfl" : /*  */ : "r" (x) : "memory")

#define sti() \
    asm volatile("sti" : : : "memory")

#define cli() \
    asm volatile("cli" : : : "memory")

#define irq_save(x) \
    { flags_save(x); cli(); }

#define irq_restore(x) \
    { flags_restore(x); }

#define nop() \
    asm volatile("nop")

#define halt() \
    ({ asm volatile("hlt"); 0; })

// Context switching
#define process_switch(prev, next) \
    do {                                \
        asm volatile(                   \
            "push %%gs;"                \
            "pushl %%ebx;"              \
            "pushl %%ecx;"              \
            "pushl %%esi;"              \
            "pushl %%edi;"              \
            "pushl %%ebp;"              \
            "movl %%esp, %0;"           \
            "movl %2, %%esp;"           \
            "movl $1f, %1;"             \
            "pushl %3;"                 \
            "jmp __process_switch;"     \
            "1: "                       \
            "popl %%ebp;"               \
            "popl %%edi;"               \
            "popl %%esi;"               \
            "popl %%ecx;"               \
            "popl %%ebx;"               \
            "pop %%gs;"                 \
            : "=m" (prev->context.esp), \
              "=m" (prev->context.eip)  \
            : "m" (next->context.esp),  \
              "m" (next->context.eip),  \
              "a" (prev), "d" (next));  \
    } while (0)
