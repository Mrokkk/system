#include <kernel/kernel.h>
#include <kernel/irq.h>
#include <kernel/time.h>
#include <arch/io.h>
#include <arch/pit.h>

static struct irq irq_list[16];

static unsigned short mask = 0xffff;

#define PIC1 0x20
#define PIC2 0xA0

#define ICW2_PIC1   0x20
#define ICW2_PIC2   0x28

#define CLOCK_TICK_RATE 1193182
#define LATCH  ((CLOCK_TICK_RATE) / HZ) /* TODO: Why is it so inaccurate? */

void pic_disable();
static void empty_isr() {};

/*===========================================================================*
 *                               irq_register                                *
 *===========================================================================*/
int irq_register(unsigned int nr, void (*handler)(), const char *name) {

    if (irq_list[nr].handler)
        return -EBUSY;

    irq_list[nr].handler = handler;
    irq_list[nr].name = name;

    irq_enable(nr);

    return 0;

}

/*===========================================================================*
 *                                  do_irq                                   *
 *===========================================================================*/
void do_irq(unsigned int nr, struct pt_regs *regs) {

    if (irq_list[nr].handler) {
        irq_list[nr].handler(nr, regs);
        return;
    }

    printk("Not handled INT 0x%x", nr);

}

/*===========================================================================*
 *                               irqs_list_get                               *
 *===========================================================================*/
int irqs_list_get(char *buffer) {

    int len, i = 0;

    len = sprintf(buffer, "Registered IRQ's:\n");

    for (i = 0; i < 16; i++)
        if (irq_list[i].handler)
            len += sprintf(buffer + len, "%d : %s\n", i, irq_list[i].name);

    return len;

}

/*===========================================================================*
 *                               pit_configure                               *
 *===========================================================================*/
void pit_configure() {

    outb(PIT_CHANNEL0 | PIT_MODE_2 | PIT_ACCES_LOHI | PIT_16BIN,
            PIT_PORT_COMMAND);
    outb(LATCH & 0xff, PIT_PORT_CHANNEL0);
    outb(LATCH >> 8, PIT_PORT_CHANNEL0);

    irq_enable(0);

}

/*===========================================================================*
 *                                irq_enable                                 *
 *===========================================================================*/
void irq_enable(unsigned int irq) {

    unsigned int flags;

    mask &= ~(1 << irq);

    irq_save(flags);

    if (irq < 8)
        outb(mask & 0xff, PIC1 + 1);
    else
        outb(mask >> 8, PIC2 + 1);

    irq_restore(flags);

}

/*===========================================================================*
 *                               irq_disable                                 *
 *===========================================================================*/
void irq_disable(int irq) {

    unsigned int flags;

    mask |= (1 << irq);

    irq_save(flags);

    if (irq < 8)
        outb(mask & 0xFF, PIC1 + 1);
    else
        outb(mask >> 8, PIC2 + 1);

    irq_restore(flags);

}

/*===========================================================================*
 *                               pic_disable                                 *
 *===========================================================================*/
void pic_disable() {

    unsigned int flags;

    irq_save(flags);

    outb(0xff, PIC1 + 1);
    outb(0xff, PIC2 + 1);

    irq_restore(flags);

}

/*===========================================================================*
 *                                pic_enable                                 *
 *===========================================================================*/
void pic_enable() {

    unsigned int flags;

    irq_save(flags);

    outb(mask & 0xff, PIC1 + 1);
    outb((mask >> 8) & 0xff, PIC2 + 1);

    irq_restore(flags);

}

/*===========================================================================*
 *                                 icw1_send                                 *
 *===========================================================================*/
static inline void icw1_send() {
    outb(0x11, PIC1);
    outb(0x11, PIC2);
}

/*===========================================================================*
 *                                 icw2_send                                 *
 *===========================================================================*/
static inline void icw2_send() {
    outb(ICW2_PIC1, PIC1 + 1);
    outb(ICW2_PIC2, PIC2 + 1);
}

/*===========================================================================*
 *                                 icw3_send                                 *
 *===========================================================================*/
static inline void icw3_send() {
    outb(4, PIC1 + 1);
    outb(2, PIC2 + 1);
}

/*===========================================================================*
 *                                 icw4_send                                 *
 *===========================================================================*/
static inline void icw4_send() {
    outb(1, PIC1 + 1);
    outb(1, PIC2 + 1);
}

/*===========================================================================*
 *                               irqs_configure                              *
 *===========================================================================*/
void irqs_configure() {

    icw1_send();
    icw2_send();
    icw3_send();
    icw4_send();
    pic_disable();
    pit_configure();
    irq_register(0, &empty_isr, "timer");
    irq_register(2, &empty_isr, "cascade");
    irq_register(13, &empty_isr, "fpu");

}

