#pragma once

#include <stdint.h>

#define PIC_IRQ_VECTOR_OFFSET       0x20
#define IOAPIC_IRQ_VECTOR_OFFSET    0x30
#define IRQ_CHIPS_COUNT             2

struct irq_chip;

typedef struct irq_chip irq_chip_t;

struct irq_chip
{
    void (*eoi)(uint32_t irq);
    int (*initialize)(void);
    int (*irq_enable)(uint32_t irq, int flags);
    int (*irq_disable)(uint32_t irq, int flags);
    int (*disable)(void);
    const char* name;
    uint8_t vector_offset;
};

int irq_chip_register(irq_chip_t* chip);
int irq_chip_disable(void);
