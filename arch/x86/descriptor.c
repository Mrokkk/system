#include <arch/page.h>
#include <arch/segment.h>
#include <arch/descriptor.h>

#include <kernel/kernel.h>
#include <kernel/process.h>

extern struct gdt_entry __gdt_entries[];

struct gdt_entry* gdt_entries = (struct gdt_entry*)virt_addr(__gdt_entries);

struct idt_entry idt_entries[256];

struct idt idt = {
    sizeof(struct idt_entry) * 256 - 1,
    (uint32_t)&idt_entries
};

static inline void idt_set_gate(
    uint8_t num,
    uint32_t base,
    uint16_t selector,
    uint32_t flags)
{
    idt_entries[num].base_lo = (base) & 0xFFFF;
    idt_entries[num].base_hi = ((base) >> 16) & 0xFFFF;
    idt_entries[num].sel = (selector);
    idt_entries[num].always0 = 0;
    idt_entries[num].flags = flags | 0x80;
}

void tss_init()
{
    uint32_t base = (uint32_t)&process_current->context;
    uint32_t limit = sizeof(struct context) - IO_BITMAP_SIZE;

    descriptor_set_base(gdt_entries, FIRST_TSS_ENTRY, base);
    descriptor_set_limit(gdt_entries, FIRST_TSS_ENTRY, limit);

    // Disable all ports
    for (int i = 0; i < IO_BITMAP_SIZE; i++)
    {
        process_current->context.io_bitmap[i] = 0xff;
    }

    tss_load(tss_selector(0));
}

extern void syscall_handler();

void idt_init()
{
    #define __isr(x)

    #define __pic_isr(x) \
        extern void isr_##x(); \
        idt_set_gate( \
            (x), \
            (uint32_t)isr_##x, \
            KERNEL_CS, \
            GDT_FLAGS_TYPE_32TRAP_GATE);

    #define __syscall_handler(x) \
        idt_set_gate( \
            (x), \
            (uint32_t)syscall_handler, \
            KERNEL_CS, \
            GDT_FLAGS_TYPE_32TRAP_GATE | GDT_FLAGS_RING3);

    #define __timer_isr(x) \
        void timer_handler(); \
        idt_set_gate( \
            (x), \
            (uint32_t)timer_handler, \
            KERNEL_CS, \
            GDT_FLAGS_TYPE_32TRAP_GATE);

    #define __exception_noerrno(x) \
        extern void exc_##x##_handler(); \
        idt_set_gate( \
            (__NR_##x), \
            (uint32_t)exc_##x##_handler, \
            KERNEL_CS, \
            GDT_FLAGS_TYPE_32TRAP_GATE);

    #define __exception_errno __exception_noerrno
    #define __exception_debug __exception_noerrno

    #include <arch/isr.h>
    #include <arch/exception.h>

    idt_load(&idt);
}
