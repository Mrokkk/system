#include <kernel/kernel.h>

#include <arch/descriptor.h>
#include <arch/segment.h>
#include <arch/page.h>

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

extern struct gdt_entry __gdt_entries[];

struct gdt_entry *gdt_entries =
        (struct gdt_entry *)((unsigned int)__gdt_entries + KERNEL_PAGE_OFFSET);

struct idt_entry idt_entries[256];

struct idt idt = {
        sizeof(struct idt_entry) * 256 - 1,
        (unsigned int)&idt_entries
};

/*===========================================================================*
 *                               idt_set_gate                                *
 *===========================================================================*/
static inline void idt_set_gate(unsigned char num, unsigned long base,
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
void tss_init() {

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
void idt_configure() {

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

