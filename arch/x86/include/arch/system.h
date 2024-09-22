#pragma once

#include <stdint.h>
#include <arch/segment.h>
#include <kernel/debug.h>
#include <kernel/compiler.h>

typedef uintptr_t flags_t;

#define mb() \
    asm volatile("" : : : "memory")

#define flags_save(x) \
    asm volatile("pushf; pop %0" : "=rm" (x) :: "memory")

#define flags_restore(x) \
    asm volatile("push %0; popf" :: "g" (x) : "memory", "cc")

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

#define rdtscp(hi, low, c) \
    asm volatile("rdtscp" : "=a" (low), "=c" (c), "=d" (hi))

#define rdmsr(msr, low, high) \
    asm volatile("rdmsr" : "=a" (low), "=d" (high) : "c" (msr))

#define wrmsr(msr, low, high) \
    asm volatile("wrmsr" :: "a" (low), "d" (high), "c" (msr))

#define cpu_relax() \
    asm volatile("rep; nop" ::: "memory")

static inline void __flags_restore(flags_t* flags)
{
    flags_restore(*flags);
}

#define __flags_save()      ({ flags_t f; irq_save(f); f; })
#define scoped_flags_t      CLEANUP(__flags_restore) flags_t
#define scoped_irq_lock()   scoped_flags_t __flags = __flags_save(); (void)__flags

extern void exit_kernel(void);
extern void context_restore(void);

void panic_mode_enter(void);
void panic_mode_die(void);
