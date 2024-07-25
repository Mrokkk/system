#pragma once

#ifndef __ASSEMBLER__

#include <arch/processor.h>
#include <kernel/compiler.h>

struct gdt_entry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} PACKED;

typedef struct gdt_entry gdt_entry_t;

struct gdt
{
    uint16_t limit;
    uint32_t base;
} PACKED;

typedef struct gdt gdt_t;

struct idt_entry
{
   uint16_t base_lo;
   uint16_t sel;
   uint8_t always0;
   uint8_t flags;
   uint16_t base_hi;
} PACKED;

typedef struct idt_entry idt_entry_t;

struct idt
{
   uint16_t limit;
   uint32_t base;
} PACKED;

typedef struct idt idt_t;

extern gdt_entry_t* gdt_entries;

static inline void idt_load(idt_t* idt)
{
    asm volatile(
        "lidt (%%eax);"
        :: "a" (idt)
    );
}

static inline void idt_store(idt_t* idt)
{
    asm volatile(
        "sidt (%%eax);"
        :: "a" (idt)
    );
}

static inline void tss_load(uint16_t sel)
{
    asm volatile(
        "ltr %%ax;"
        :: "a" (sel)
    );
}

static inline void tss_store(uint16_t* sel)
{
    asm volatile(
        "str %%ax;"
        : "=a" (*sel)
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

static inline void gdt_store(gdt_t* gdt)
{
    asm volatile(
        "sgdt (%%eax);"
        :: "a" (gdt)
    );
}

void tss_init(void);
void idt_init(void);
void idt_set(int nr, uint32_t addr);
uint32_t idt_get(int nr);
void idt_write_protect(void);

#endif // !__ASSEMBLER__

#define TSS_ENTRY           5
#define APM_CODE_ENTRY      6
#define APM_CODE_16_ENTRY   7
#define APM_DATA_ENTRY      8
#define TIMER_IDT_ENTRY     32

#define IDT_OFFSET          2048

#define descriptor_selector(num, ring) ((num << 3) | ring)
#define TSS_SELECTOR ((TSS_ENTRY) << 3)

#define GDT_LOW_LIMIT(limit) \
    ((limit) & 0xFFFF)

#define GDT_HI_LIMIT(limit) \
    (((limit) >> 16) & 0xF)

#define GDT_LOW_BASE(base) \
    ((base) & 0xFFFF)

#define GDT_MID_BASE(base) \
    (((base) >> 16) & 0xff)

#define GDT_HI_BASE(base) \
    (((base) >> 24) & 0xff)

#define GDT_LOW_FLAGS(flags) \
    (((flags) & 0x7F) & 0xff )

#define GDT_HI_FLAGS(flags) \
    (((flags) >> 1) & 0xf0)

// flags has following layout:
//
// MSB                      LSB
// 9   8   7  6  5   4      0
// [ G | D | DPL | S | TYPE ]
//
// Note: TYPE includes also access bit (A)

#ifndef __ASSEMBLER__

#define descriptor_entry(flags, base, limit) \
    { \
        GDT_LOW_LIMIT(limit), \
        GDT_LOW_BASE(base), \
        GDT_MID_BASE(base), \
        GDT_LOW_FLAGS(flags) | (1 << 7), \
        GDT_HI_LIMIT(limit) | GDT_HI_FLAGS(flags), \
        GDT_HI_BASE(base) \
    }

#define descriptor_set_base(gdt, num, base) \
    gdt[num].base_low = (base) & 0xffff; \
    gdt[num].base_middle = (((base) >> 16)) & 0xff; \
    gdt[num].base_high = ((base) >> 24) & 0xff;

#define descriptor_set_limit(gdt, num, limit) \
    gdt[num].limit_low = (limit) & 0xffff; \
    gdt[num].granularity = (((limit) >> 16) & 0xf);

#define descriptor_get_base(gdt, num) \
    (gdt[num].base_low | (gdt[num].base_middle << 16) | (gdt[num].base_high << 24))

#define descriptor_get_limit(gdt, num) \
    (gdt[num].limit_low | (gdt[num].granularity & 0xf) << 16)

#define descriptor_get_type(gdt, num) \
    (gdt[num].access & 0x1e)

#else // __ASSEMBLER__

#define descriptor_entry(flags, base, limit) \
        .word GDT_LOW_LIMIT(limit); \
        .word GDT_LOW_BASE(base); \
        .byte GDT_MID_BASE(base); \
        .byte GDT_LOW_FLAGS(flags) | (1 << 7); \
        .byte GDT_HI_LIMIT(limit) | GDT_HI_FLAGS(flags); \
        .byte GDT_HI_BASE(base);

#endif // !__ASSEMBLER__

// DPL in flags
#define GDT_FLAGS_RING0         (0 << 5)
#define GDT_FLAGS_RING1         (1 << 5)
#define GDT_FLAGS_RING2         (2 << 5)
#define GDT_FLAGS_RING3         (3 << 5)

// G in flags
#define GDT_FLAGS_4KB           (1 << 8)
#define GDT_FLAGS_1B            (0 << 8)

// D in flags
#define GDT_FLAGS_16BIT         (0 << 7)
#define GDT_FLAGS_32BIT         (1 << 7)

// TYPE
#define GDT_FLAGS_TYPE_CODE         (0x1a)
#define GDT_FLAGS_TYPE_DATA         (0x12)
#define GDT_FLAGS_TYPE_16TSS        (0x1)
#define GDT_FLAGS_TYPE_32TSS        (0x9)
#define GDT_FLAGS_TYPE_16INT_GATE   (0x6)
#define GDT_FLAGS_TYPE_32INT_GATE   (0xe)
#define GDT_FLAGS_TYPE_16TRAP_GATE  (0x7)
#define GDT_FLAGS_TYPE_32TRAP_GATE  (0xf)
#define GDT_FLAGS_TYPE_TASK_GATE    (0x5)
