#include <arch/segment.h>
#include <arch/descriptor.h>

#include <kernel/kernel.h>
#include <kernel/page_types.h>

#ifdef __i386__
#include <kernel/process.h>
#endif

struct idt_data
{
    union
    {
        idt_t idt;
        uint8_t padding[16];
    };
    idt_entry_t entries[256];
};

typedef struct idt_data idt_data_t;

READONLY idt_data_t ALIGN(8) idt = {
    .idt = {
        .limit = sizeof(idt_entry_t) * 256 - 1,
        .base = addr(&idt.entries),
    },
    .entries = {}
};

tss_t tss;

static inline void idt_set_gate(uint8_t num, uintptr_t base, uint16_t selector, uint32_t flags)
{
    idt.entries[num].base0 = base & 0xffff;
    idt.entries[num].base1 = (base >> 16) & 0xffff;
    idt.entries[num].sel = selector;
    idt.entries[num].reserved = 0;
    idt.entries[num].access = flags | 0x80;

#ifdef __x86_64__
    idt.entries[num].base2 = ((base) >> 32) & 0xffffffff;
    idt.entries[num].ist = 0;
#endif
}

UNMAP_AFTER_INIT void tss_init()
{
    uintptr_t base = addr(&tss);
    uint16_t limit = sizeof(tss_t) - IO_BITMAP_SIZE;

#ifdef __i386__
    descriptor_set_base(gdt_entries, TSS_ENTRY, base);
#else
    descriptor_set_base64(gdt_entries, TSS_ENTRY, base);
#endif
    descriptor_set_limit(gdt_entries, TSS_ENTRY, limit);

    tss.iomap_offset = IOMAP_OFFSET;
#ifdef __i386__
    tss.ss0 = KERNEL_DS;
    tss.ss2 = USER_DS;
    tss.esp = init_process.context.esp;
#endif

    // Disable all ports
    memset(tss.io_bitmap, 0xff, IO_BITMAP_SIZE);

    tss_load();
}

void idt_set(int nr, uintptr_t addr)
{
    idt_set_gate(nr, addr, KERNEL_CS, DESC_ACCESS_32TRAP_GATE);
}

UNMAP_AFTER_INIT void idt_init()
{
    #define __pic_isr(x) \
        extern void isr_##x(); \
        idt_set_gate( \
            x, \
            addr(&isr_##x), \
            KERNEL_CS, \
            DESC_ACCESS_32TRAP_GATE);

    #define __apic_isr __pic_isr

    #define __syscall_handler(x) \
        extern void syscall_handler(); \
        idt_set_gate( \
            x, \
            addr(&syscall_handler), \
            KERNEL_CS, \
            DESC_ACCESS_32TRAP_GATE | DESC_ACCESS_RING(3));

    #define __timer_isr(x) \
        extern void timer_handler(); \
        idt_set_gate( \
            x, \
            addr(&timer_handler), \
            KERNEL_CS, \
            DESC_ACCESS_32TRAP_GATE);

    #define __exception(x, nr, ...) \
        extern void exc_##x##_handler(); \
        idt_set_gate( \
            nr, \
            addr(&exc_##x##_handler), \
            KERNEL_CS, \
            DESC_ACCESS_32TRAP_GATE);

    #define __exception_debug __exception

    #include <arch/isr.h>
    #include <arch/exception.h>

    idt_load(&idt.idt);
}
