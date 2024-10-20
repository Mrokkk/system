#pragma once

#include <arch/descriptor_common.h>

#define IDT_OFFSET 2048

#define APM_CODE_ENTRY      5
#define APM_CODE_16_ENTRY   6
#define APM_DATA_ENTRY      7
#define TLS_ENTRY           8
#define KERNEL_16_CODE      9
#define KERNEL_16_DATA      10
#define TSS_ENTRY           11

#ifndef __ASSEMBLER__

#include <arch/processor.h>
#include <kernel/compiler.h>

struct gdt
{
    uint16_t limit;
    uint32_t base;
} PACKED;

typedef struct gdt gdt_t;

struct idt
{
    uint16_t limit;
    uint32_t base;
} PACKED;

typedef struct idt idt_t;

struct idt_entry
{
    uint16_t base0;
    uint16_t sel;
    uint8_t  reserved;
    uint8_t  access;
    uint16_t base1;
} PACKED;

typedef struct idt_entry idt_entry_t;

#endif // !__ASSEMBLER__
