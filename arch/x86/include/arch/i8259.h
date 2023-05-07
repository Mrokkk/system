#pragma once

// Reference: https://pdos.csail.mit.edu/6.828/2005/readings/hardware/8259A.pdf

#define PIC1_CMD        0x20
#define PIC1_DATA       0x21
#define PIC2_CMD        0xa0
#define PIC2_DATA       0xa1

#define PIC_EOI_VAL     0x20
#define PIC_CASCADE_IR  2

#define ICW1_ICW4       0x01        // Indicates that ICW4 will be present
#define ICW1_SINGLE     0x02        // Single mode
#define ICW1_INTERVAL4  0x04        // Call address interval 4 (8)
#define ICW1_LEVEL      0x08        // Level triggered (edge) mode
#define ICW1_INIT       0x10        // Initialization

#define ICW2_PIC1_OFF   0x20        // PIC1 IRQ offset
#define ICW2_PIC2_OFF   0x28        // PIC2 IRQ offset

#define ICW3_PIC1_ID    (1 << PIC_CASCADE_IR)
#define ICW3_PIC2_ID    PIC_CASCADE_IR

#define ICW4_8086       0x01        // 8086/88 (MCS-80/85) mode
#define ICW4_AEOI       0x02        // Auto-EOI
#define ICW4_BUF_SLAVE  0x08        // Buffered mode/slave
#define ICW4_BUF_MASTER 0x0C        // Buffered mode/master
#define ICW4_SFNM       0x10        // Special fully nested mode

#define OCW2_SPEC_EOI   (3 << 5)

#ifndef __ASSEMBLER__

void i8259_preinit(void);
int i8259_disable(void);
void i8259_enable(void);
void i8259_check(void);

extern bool i8259_used;

#endif // __ASSEMBLER__
