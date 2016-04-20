#include <kernel/kernel.h>
#include <arch/descriptor.h>
#include <arch/apm.h>
#include <arch/bios.h>
#include <arch/multiboot.h>

struct multiboot_apm_table_struct *apm_table;

/* TODO: Errors handler */

struct {
    unsigned int offset;
    unsigned short selector;
} __attribute__((packed)) apm_entry;

/*===========================================================================*
 *                               apm_make_call                               *
 *===========================================================================*/
struct regs_struct *apm_make_call(struct regs_struct *regs) {

    if (apm_table)
        apm_call(regs);
    else
        execute_in_real_mode((unsigned int)&bios_int15h, regs);

    return regs;
}

/*===========================================================================*
 *                               apm_regs_init                               *
 *===========================================================================*/
struct regs_struct *apm_regs_init(unsigned int apm_function,
                                  struct regs_struct *regs) {

    unsigned char nr = apm_function & 0xF;

    regs->ah = 0x53;

    switch (nr) {
        case __APM_CONNECT:
            regs->al = (apm_function >> 4) & 0x3;
            regs->bx = 0x0000;
            break;
        case __APM_DISCONNECT:
            regs->al = 0x04;
            regs->bx = 0x0000;
            break;
        case __APM_POWER_MANAGEMENT_ENABLE:
            regs->al = 0x08;
            regs->bx = 0xffff; /* 0x0001 ??? */
            regs->cx = 0x1;
            break;
        case __APM_POWER_MANAGEMENT_DISABLE:
            regs->al = 0x08;
            regs->bx = 0xffff; /* 0x0001 ??? */
            regs->cx = 0x0;
            break;
        case __APM_POWER_STATE_SET:
            regs->al = 0x07;
            regs->bx = (apm_function >> 12) & 0xff;
            regs->cx = (apm_function >> 4) & 0x3;
            break;
    }

    return regs;

}

/*===========================================================================*
 *                                apm_enable                                 *
 *===========================================================================*/
void apm_enable() {

    struct regs_struct regs;

    if (!apm_table) {

        printk("Getting apm_table...\n");

        if (apm_check(&regs)->eflags & 1) return;

        apm_table->flags = regs.cx & 0xffff;
        apm_table->version = regs.ax & 0xffff;

        if (!(apm_table->flags & 0x2)) {
            printk("APM does not support 32bit interface\n");
            return;
        }

        if (apm_connect(&regs, APM_INTERFACE_32BIT_PROTECTED))
            printk("There was a problem connecting to 32 bit APM\n");

        apm_table = kmalloc(sizeof(struct multiboot_apm_table_struct));

        apm_table->cseg = regs.ax;
        apm_table->offset = regs.ebx;
        apm_table->cseg_16 = regs.cx;
        apm_table->dseg = regs.dx;
        apm_table->cseg_len = regs.esi & 0xffff;
        apm_table->cseg_16_len = regs.esi >> 16;
        apm_table->dseg_len = regs.di & 0xffff;

    }

    descriptor_set_base(gdt_entries, 5, apm_table->cseg << 4);
    descriptor_set_base(gdt_entries, 6, apm_table->cseg_16 << 4);
    descriptor_set_base(gdt_entries, 7, apm_table->dseg << 4); /* TODO: ?? */

    apm_entry.offset = apm_table->offset;
    apm_entry.selector = descriptor_selector(FIRST_APM_ENTRY, 0);

    if (apm_table->cseg != apm_table->dseg) {
        printk("Probably problem with APM!\n");
        /* TODO: Check in flags, if APM supports 32bit */
        if (apm_power_management_enable(&regs)->eflags & 1)
            printk("Fucking APM!\n");
    } else {
        apm_power_management_enable(&regs);
    }

}

#define APM_DISABLED    0x01
#define APM_REAL_EST    0x02
#define APM_NCONNECT    0x03
#define APM_16BIT_EST   0x05
#define APM_NO16BIT     0x06
#define APM_32BIT_EST   0x07
#define APM_NO32BIT     0x08
#define APM_NODEV       0x09
#define APM_BOUNDS      0x0a
#define APM_NENGAGED    0x0b


void apm_error(unsigned char errno) {

    (void)errno;

}
