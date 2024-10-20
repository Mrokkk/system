#pragma once

#include <arch/descriptor_common.h>

#define IDT_OFFSET 4096

#define APM_CODE_ENTRY       5
#define APM_CODE_16_ENTRY    6
#define APM_DATA_ENTRY       7
#define TLS_ENTRY            8
#define KERNEL_32_CODE       9
#define KERNEL_16_CODE       10
#define KERNEL_16_DATA       11
#define TSS_ENTRY            12

#ifndef __ASSEMBLER__

#include <arch/processor.h>
#include <kernel/compiler.h>

struct gdt
{
    uint16_t limit;
    uint64_t base;
} PACKED;

typedef struct gdt gdt_t;

struct idt
{
    uint16_t limit;
    uint64_t base;
} PACKED;

typedef struct idt idt_t;

struct idt_entry
{
    uint16_t base0;
    uint16_t sel;
    uint8_t  ist;
    uint8_t  access;
    uint16_t base1;
    uint32_t base2;
    uint32_t reserved;
} PACKED;

typedef struct idt_entry idt_entry_t;

#define descriptor_set_base64(gdt, num, base) \
    gdt[num].base_low = (base) & 0xffff; \
    gdt[num].base_middle = (((base) >> 16)) & 0xff; \
    gdt[num].base_high = ((base) >> 24) & 0xff; \
    *((volatile uint32_t*)gdt[num].extended) = (base) >> 32;

#endif // !__ASSEMBLER__
