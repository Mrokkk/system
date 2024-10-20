#include <arch/segment.h>
#include <arch/descriptor.h>

#include <kernel/page.h>
#include <kernel/kernel.h>

#ifdef __i386__
#include <kernel/process.h>
#endif

struct idt_data
{
    union
    {
        struct
        {
            idt_entry_t entries[256];
            idt_t idt;
        };
        uint8_t padding[page_align(256 * sizeof(idt_entry_t) + sizeof(idt_t))];
    };
}
ALIGN(PAGE_SIZE) idt = {
    .entries = {},
    .idt = {
        .limit = sizeof(idt_entry_t) * 256 - 1,
        .base = addr(&idt.entries),
    },
};

tss_t tss;

static_assert(offsetof(struct idt_data, idt) == IDT_OFFSET);

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

#ifdef __i386__
UNMAP_AFTER_INIT void idt_write_protect()
{
    uint32_t address = addr(&idt);
    uint32_t pde_index = pde_index(address);
    uint32_t pte_index = pte_index(address);

    pgt_t* pgt = virt_ptr(init_pgd_get()[pde_index] & PAGE_ADDRESS);

    pgt[pte_index] = (pgt[pte_index] & PAGE_ADDRESS) | PTE_PRESENT;

    pgd_reload();
}
#endif

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
