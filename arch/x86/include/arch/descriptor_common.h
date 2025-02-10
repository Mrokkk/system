#pragma once

#ifdef __ASSEMBLER__

#define descriptor_entry(access, flags, base, limit) \
    .word GDT_LOW_LIMIT(limit); \
    .word GDT_LOW_BASE(base); \
    .byte GDT_MID_BASE(base); \
    .byte access | (1 << 7); \
    .byte GDT_HI_LIMIT(limit) | (flags) << 4; \
    .byte GDT_HI_BASE(base);

#endif

#define descriptor_selector(num, ring) ((num << 3) | ring)

#define GDT_LOW_LIMIT(limit) \
    ((limit) & 0xffff)

#define GDT_HI_LIMIT(limit) \
    (((limit) >> 16) & 0xf)

#define GDT_LOW_BASE(base) \
    ((base) & 0xffff)

#define GDT_MID_BASE(base) \
    (((base) >> 16) & 0xff)

#define GDT_HI_BASE(base) \
    (((base) >> 24) & 0xff)

// DPL in access
#define DESC_ACCESS_RING(r)         ((r) << 5)

// G in flags
#define DESC_FLAGS_4KB              (1 << 3)
#define DESC_FLAGS_1B               (0 << 3)

// DB in flags
#define DESC_FLAGS_16BIT            (0 << 2)
#define DESC_FLAGS_32BIT            (1 << 2)

#define DESC_ACCESS_CODE            ((1 << 1) | (1 << 3) | (1 << 4))
#define DESC_ACCESS_DATA            ((1 << 1) | (1 << 4))
#define DESC_ACCESS_TSS             0x9

#define DESC_ACCESS_16INT_GATE      0x6
#define DESC_ACCESS_32INT_GATE      0xe
#define DESC_ACCESS_16TRAP_GATE     0x7
#define DESC_ACCESS_32TRAP_GATE     0xf

#define DESC_FLAGS_64BIT            (1 << 1)
#define DESC_FLAGS_16BIT            (0 << 2)
#define DESC_FLAGS_32BIT            (1 << 2)
#define DESC_FLAGS_1B               (0 << 3)
#define DESC_FLAGS_4KB              (1 << 3)

#ifndef __ASSEMBLER__

#define descriptor_set_base(gdt, num, base) \
    gdt[num].base_low = (base) & 0xffff; \
    gdt[num].base_middle = (((base) >> 16)) & 0xff; \
    gdt[num].base_high = ((base) >> 24) & 0xff

#define descriptor_set_limit(gdt, num, limit) \
    gdt[num].limit_low = (limit) & 0xffff; \
    gdt[num].granularity &= ~0xf; \
    gdt[num].granularity |= (((limit) >> 16) & 0xf)

#include <stdint.h>
#include <kernel/compiler.h>

typedef struct idt idt_t;
typedef struct gdt gdt_t;

typedef struct gdt_entry gdt_entry_t;
typedef struct idt_entry idt_entry_t;

struct gdt_entry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t extended[0];
} PACKED;

#endif // !__ASSEMBLER__
