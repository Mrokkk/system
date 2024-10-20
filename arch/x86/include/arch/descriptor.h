#pragma once

#ifdef __i386__
#include <arch/descriptor_32.h>
#else
#include <arch/descriptor_64.h>
#endif

#ifndef __ASSEMBLER__

static inline void tss_load(void)
{
    asm volatile(
        "ltr %%ax;"
        :: "a" (descriptor_selector(TSS_ENTRY, 0))
    );
}

static inline void idt_load(idt_t* idt)
{
    asm volatile(
        "lidt (%%eax);"
        :: "a" (idt)
    );
}

static inline void gdt_load(gdt_t* gdt)
{
    asm volatile(
        "lgdt (%%eax);"
        "ljmp $0x08, $1f;"
        "1:"
        :: "a" (gdt)
    );
}

void tss_init(void);
void idt_init(void);
void idt_set(int nr, uintptr_t addr);
void idt_write_protect(void);

extern gdt_t gdt;
extern gdt_entry_t gdt_entries[];

#endif
