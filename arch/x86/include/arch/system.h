#pragma once

#include <stdint.h>
#include <arch/segment.h>
#include <kernel/trace.h>
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
    ({ asm volatile("hlt"); 1; })

#define rdtsc(low, high) \
     asm volatile("rdtsc" : "=a" (low), "=d" (high))

#define rdtscl(val) \
     asm volatile("rdtsc" : "=a" (val))

#define rdtscll(val) \
     asm volatile("rdtsc" : "=A" (val))
