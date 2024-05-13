#define log_fmt(fmt) "i8259: " fmt
#include <arch/io.h>
#include <arch/irq.h>
#include <arch/i8259.h>

#include <kernel/irq.h>
#include <kernel/kernel.h>

static int i8259_initialize(void);
static int i8259_irq_enable(uint32_t irq, int flags);
static int i8259_irq_disable(uint32_t irq, int flags);
void i8259_eoi(uint32_t irq);

static uint16_t mask = 0xffff;
bool i8259_used;

static irq_chip_t i8259 = {
    .name = "i8259",
    .initialize = &i8259_initialize,
    .irq_enable = &i8259_irq_enable,
    .irq_disable = &i8259_irq_disable,
    .disable = &i8259_disable,
    .eoi = &i8259_eoi,
    .vector_offset = PIC_IRQ_VECTOR_OFFSET,
};

static void empty_isr() {}

UNMAP_AFTER_INIT void i8259_preinit(void)
{
    irq_chip_register(&i8259);
}

UNMAP_AFTER_INIT static int i8259_initialize(void)
{
    // ICW1
    outb(ICW1_INIT | ICW1_ICW4, PIC1_CMD);
    outb(ICW1_INIT | ICW1_ICW4, PIC2_CMD);

    // ICW2: vector offset
    outb(ICW2_PIC1_OFF, PIC1_DATA);
    outb(ICW2_PIC2_OFF, PIC2_DATA);

    // ICW3: cascade
    outb(ICW3_PIC1_ID, PIC1_DATA);
    outb(ICW3_PIC2_ID, PIC2_DATA);

    // ICW4: mode
    outb(ICW4_8086, PIC1_DATA);
    outb(ICW4_8086, PIC2_DATA);

    // OCW: disable all IRQs
    outb(0xff, PIC1_DATA);
    outb(0xff, PIC2_DATA);

    irq_register(2, &empty_isr, "cascade", IRQ_DEFAULT);
    irq_register(13, &empty_isr, "fpu", IRQ_DEFAULT);

    i8259_used = true;

    return 0;
}

static int i8259_irq_enable(uint32_t irq, int)
{
    scoped_irq_lock();

    mask &= ~(1 << irq);

    if (irq < 8)
    {
        outb(mask & 0xff, PIC1_DATA);
    }
    else
    {
        outb(mask >> 8, PIC2_DATA);
    }

    return 0;
}

static int i8259_irq_disable(uint32_t irq, int)
{
    scoped_irq_lock();

    mask |= (1 << irq);

    if (irq < 8)
    {
        outb(mask & 0xFF, PIC1_DATA);
    }
    else
    {
        outb(mask >> 8, PIC2_DATA);
    }

    return 0;
}

void i8259_eoi(uint32_t irq)
{
    if (mask & (1 << irq))
    {
        // TODO: handle APIC timer + PIC properly
        if (irq == 0)
        {
            extern void apic_eoi(uint32_t);
            apic_eoi(irq);

            // FIXME: when APIC timer is used with i8259, sometimes the chip gets
            // stuck with multiple IRQs in both IRR and ISR registers and no longer
            // handles IRQs > 1. Below non-specific EOI seems to be a WA for the
            // issue.
            outb(OCW2_NONSPEC_EOI, PIC2_CMD);
            outb(OCW2_NONSPEC_EOI, PIC1_CMD);

            return;
        }
        else
        {
            // TODO: handle spurious interrupts
            panic("unsupported spurious interrupt %u", irq);
        }
    }

    if (irq & 8)
    {
        inb(PIC2_DATA);
        outb(OCW2_SPEC_EOI | (irq & 7), PIC2_CMD);
        outb(OCW2_SPEC_EOI | PIC_CASCADE_IR, PIC1_CMD);
    }
    else
    {
        inb(PIC1_DATA);
        outb(OCW2_SPEC_EOI | irq, PIC1_CMD);
    }
}

int i8259_disable(void)
{
    scoped_irq_lock();
    outb(0xff, PIC1_DATA);
    outb(0xff, PIC2_DATA);
    return 0;
}

void i8259_enable(void)
{
    scoped_irq_lock();
    outb(mask & 0xff, PIC1_DATA);
    outb((mask >> 8) & 0xff, PIC2_DATA);
}

void i8259_check(void)
{
    uint16_t data;
    data = inb(PIC1_DATA);
    data |= inb(PIC2_DATA) << 8;

    if (unlikely(data != mask))
    {
        log_warning("mask configured in controller: %x, expected: %x; reconfiguring");
        i8259_enable();
    }
}
