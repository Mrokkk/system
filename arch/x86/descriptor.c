#include <kernel/kernel.h>

#include <arch/descriptor.h>
#include <arch/segment.h>

/* Exceptions */
#define __exception_noerrno(x) void exc_##x##_handler();
#define __exception_errno(x) void exc_##x##_handler();
#define __exception_debug(x) void exc_##x##_handler();
#include <arch/exception.h>

/* Interrupts */
#define __isr(x)
#define __pic_isr(x) void isr_##x();
#define __timer_isr(x) void timer_handler();
#define __syscall_handler(x) void syscall_handler();
#include <arch/isr.h>


struct gdt_entry gdt_entries[] = {

    /* Null segment */
    descriptor_entry(0, 0, 0),

    /* Kernel code segment */
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0x0, 0xffffffff),

    /* Kernel data segment */
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0x0, 0xffffffff),

    /* User code segment */
    descriptor_entry(GDT_FLAGS_RING3 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0x0, 0xffffffff),

    /* User data segment */
    descriptor_entry(GDT_FLAGS_RING3 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_4KB | GDT_FLAGS_32BIT, 0x0, 0xffff),

    /* TSS */
    descriptor_entry(GDT_FLAGS_TYPE_32TSS, 0, 0),

    /* APM code (32bit) */
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_1B | GDT_FLAGS_32BIT, 0, 0xffffffff),

    /* APM code (16bit) */
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_CODE | GDT_FLAGS_1B | GDT_FLAGS_16BIT, 0, 0xffffffff),

    /* APM data */
    descriptor_entry(GDT_FLAGS_RING0 | GDT_FLAGS_TYPE_DATA | GDT_FLAGS_1B | GDT_FLAGS_32BIT, 0, 0xffffffff),

    /* LDT #1 */
    /* LDT #2 */
    /* ...... */

};

struct gdt gdt = {
        sizeof(struct gdt_entry) * 4096 - 1,
        (unsigned long)&gdt_entries
};

struct idt_entry idt_entries[256];

struct idt idt = {
        sizeof(struct idt_entry) * 256 - 1,
        (unsigned long)&idt_entries
};

struct gdt old_gdt = {
        0,
        0
};

struct gdt_entry *old_gdt_entries;

/*===========================================================================*
 *                                 gdt_print                                 *
 *===========================================================================*/
void gdt_print(struct gdt_entry *entries, int size) {

    int i;

    for (i=0; i<size; i++) {
        printk("%u base 0x%x; ", i, descriptor_get_base(entries, i));
        printk("limit 0x%x; ", descriptor_get_limit(entries, i));
        printk("type 0x%x\n", descriptor_get_type(entries, i));
    }

}

/*===========================================================================*
 *                               idt_set_gate                                *
 *===========================================================================*/
static void idt_set_gate(unsigned char num, unsigned long base,
                         unsigned short selector, unsigned long flags) {

    idt_entries[num].base_lo = (base) & 0xFFFF;
    idt_entries[num].base_hi = ((base) >> 16) & 0xFFFF;
    idt_entries[num].sel = (selector);
    idt_entries[num].always0 = 0;
    idt_entries[num].flags = flags | 0x80;

}

/*===========================================================================*
 *                                 tss_init                                  *
 *===========================================================================*/
static void tss_init() {

    unsigned int base = (unsigned int)&process_current->context;
    unsigned int limit = sizeof(struct tss) - 128; /* Set once */
    int i;

    descriptor_set_base(gdt_entries, FIRST_TSS_ENTRY, base);
    descriptor_set_limit(gdt_entries, FIRST_TSS_ENTRY, limit);

    /* Disable all ports */
    for (i=0; i<128; i++)
        process_current->context.io_bitmap[i] = 0xff;

    tss_load(tss_selector(0));

}

/*===========================================================================*
 *                              idt_configure                                *
 *===========================================================================*/
static void idt_configure() {

    #undef __isr
    #undef __pic_isr
    #undef __timer_isr
    #undef __syscall_handler
    #define __isr(x)
    #define __pic_isr(x) \
        idt_set_gate((x), (unsigned long)isr_##x, KERNEL_CS, \
                     GDT_FLAGS_TYPE_32TRAP_GATE);
    #define __syscall_handler(x) \
        idt_set_gate((x), (unsigned long)syscall_handler, \
                 KERNEL_CS, GDT_FLAGS_TYPE_32TRAP_GATE | GDT_FLAGS_RING3);
    #define __timer_isr(x) \
        idt_set_gate((x), (unsigned long)timer_handler, \
                     KERNEL_CS, GDT_FLAGS_TYPE_32TRAP_GATE);

    #include <arch/isr.h>

    #undef __exception_noerrno
    #undef __exception_errno
    #undef __exception_debug
    #define __exception_noerrno(x) \
        idt_set_gate((__NR_##x), (unsigned long)exc_##x##_handler, \
                     KERNEL_CS, GDT_FLAGS_TYPE_32TRAP_GATE);
    #define __exception_errno __exception_noerrno
    #define __exception_debug __exception_noerrno
    #include <arch/exception.h>

    idt_load(&idt);

}

void descriptor_init() {

    /* Store descriptors set by the bootloader */
    gdt_store(&old_gdt);
    old_gdt_entries = (struct gdt_entry *)old_gdt.base;

    gdt_load(&gdt);
    idt_configure();
    tss_init();

}

