#pragma once

#include <kernel/string.h>
#include <kernel/kernel.h>

#define IRQ_NORMAL  0
#define IRQ_NAKED   1
#define IRQ_ENABLE  2

#define IRQ_DEFAULT (IRQ_NORMAL | IRQ_ENABLE)

int irqs_initialize(void);
int irq_register(uint32_t nr, void (*handler)(), const char* name, int flags);
int irq_enable(uint32_t irq);
int irq_disable(uint32_t irq);
