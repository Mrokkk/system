#pragma once

#include <kernel/string.h>
#include <kernel/kernel.h>

#define IRQ_ENABLE  1

#define IRQ_DEFAULT IRQ_ENABLE

typedef void (*irq_handler_t)(uint32_t, void*, pt_regs_t*);
typedef void (*irq_naked_handler_t)(void);

int irqs_initialize(void);
int irq_register(uint32_t nr, irq_handler_t handler, const char* name, int flags, void* private);
int irq_register_naked(uint32_t nr, irq_naked_handler_t handler, const char* name, int flags);
int irq_allocate(irq_handler_t handler, const char* name, int flags, void* private, int* irq_vector);
int irq_enable(uint32_t irq);
int irq_disable(uint32_t irq);
