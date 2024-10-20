#pragma once

#define CODE_ADDRESS        0x500
#define PROC_ADDRESS        0x700
#define IDT_ADDRESS         0x740
#define GDT_ADDRESS         0x750
#define SAVED_STACK_ADDRESS 0x760
#define PARAMS_ADDRESS      0x770
#define PARAMS_SIZE         28
#define STACK_TOP           0x1000

#ifndef __ASSEMBLER__

#include <stdint.h>

struct low_mem
{
    const uint8_t real_mode_ivt[0x400];
    const uint8_t bda[0x100];
    uint8_t       code[0x200];
    uint8_t       real_mode_code[0x40];
    uint8_t       idt[0x10];
    uint8_t       gdt[0x10];
    uint8_t       saved_stack[0x10];
    uint8_t       params[28];
};

typedef struct low_mem low_mem_t;

void real_mode_call_init(void);

#endif
