#ifndef __IRQ_H_
#define __IRQ_H_

#include <kernel/string.h>
#include <kernel/kernel.h>

struct irq {
    void (*handler)();
    const char *name;
};

int irq_register(unsigned int, void (*)(), const char *);
int irq_mask(unsigned int);
int irq_unmask(unsigned int);
int irq_request(unsigned int irq_n);
int irqs_list_get(char *string);

#define NO_IRQ  0
#define IRQ_OFF 1
#define IRQ_ON  2

#endif
