#include <kernel/kernel.h>
#include <kernel/irq.h>
#include <arch/io.h>

static struct irq irq_list[16];

static unsigned short mask = 0xffff;

#define PIC1 0x20
#define PIC2 0xA0

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
 *                               irqs_configure                              *
 *===========================================================================*/
void irqs_configure() {

    unsigned char pic1 = 0x20;
    unsigned char pic2 = 0x28;

    /* Send ICW1 */
    outb(0x11, PIC1);
    outb(0x11, PIC2);

    /* Send ICW2 */
    outb(pic1, PIC1 + 1);
    outb(pic2, PIC2 + 1);

    /* Send ICW3 */
    outb(4, PIC1 + 1);
    outb(2, PIC2 + 1);

    /* Send ICW4 */
    outb(1, PIC1 + 1);
    outb(1, PIC2 + 1);

    /* Disable PIC */
    outb(0xFF, PIC1 + 1);
    outb(0xFF, PIC2 + 1);

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


