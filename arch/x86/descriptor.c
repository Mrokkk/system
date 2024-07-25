#include <arch/segment.h>
#include <arch/descriptor.h>

#include <kernel/page.h>
#include <kernel/kernel.h>
#include <kernel/process.h>

extern gdt_entry_t unpaged_gdt_entries[];

gdt_entry_t* gdt_entries = virt_ptr(unpaged_gdt_entries);

struct idt_data
{
    idt_entry_t entries[256];
    idt_t idt;
    uint8_t padding[PAGE_SIZE - 256 * sizeof(idt_entry_t) - sizeof(idt_t)];
}
ALIGN(PAGE_SIZE) idt = {
    .entries = {},
    .idt = {
        .limit = sizeof(idt_entry_t) * 256 - 1,
        .base = addr(&idt.entries),
    },
    .padding = {}
};

tss_t tss;

static_assert(offsetof(struct idt_data, idt) == IDT_OFFSET);

static inline void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint32_t flags)
{
    idt.entries[num].base_lo = (base) & 0xFFFF;
    idt.entries[num].base_hi = ((base) >> 16) & 0xFFFF;
    idt.entries[num].sel = selector;
    idt.entries[num].always0 = 0;
    idt.entries[num].flags = flags | 0x80;
}

UNMAP_AFTER_INIT void tss_init()
{
    uint32_t base = addr(&tss);
    uint32_t limit = sizeof(tss_t) - IO_BITMAP_SIZE;

    descriptor_set_base(gdt_entries, TSS_ENTRY, base);
    descriptor_set_limit(gdt_entries, TSS_ENTRY, limit);

    tss.iomap_offset = IOMAP_OFFSET;
    tss.ss0 = KERNEL_DS;
    tss.ss2 = USER_DS;
    tss.esp = init_process.context.esp;

    // Disable all ports
    memset(tss.io_bitmap, 0xff, IO_BITMAP_SIZE);

    tss_load(TSS_SELECTOR);
}

void idt_set(int nr, uint32_t addr)
{
    idt_set_gate(nr, addr, KERNEL_CS, GDT_FLAGS_TYPE_32TRAP_GATE);
}

uint32_t idt_get(int nr)
{
    return (addr(idt.entries[nr].base_hi) << 16) | idt.entries[nr].base_lo;
}

UNMAP_AFTER_INIT void idt_write_protect()
{
    uint32_t address = addr(&idt);
    uint32_t pde_index = pde_index(address);
    uint32_t pte_index = pte_index(address);

    pgt_t* pgt = virt_ptr(init_pgd_get()[pde_index] & PAGE_ADDRESS);

    pgt[pte_index] = (pgt[pte_index] & PAGE_ADDRESS) | PTE_PRESENT;

    pgd_reload();
}

UNMAP_AFTER_INIT void idt_init()
{
    #define __pic_isr(x) \
        extern void isr_##x(); \
        idt_set_gate( \
            (x), \
            addr(&isr_##x), \
            KERNEL_CS, \
            GDT_FLAGS_TYPE_32TRAP_GATE);

    #define __apic_isr __pic_isr

    #define __syscall_handler(x) \
        extern void syscall_handler(); \
        idt_set_gate( \
            (x), \
            addr(&syscall_handler), \
            KERNEL_CS, \
            GDT_FLAGS_TYPE_32TRAP_GATE | GDT_FLAGS_RING3);

    #define __timer_isr(x) \
        extern void timer_handler(); \
        idt_set_gate( \
            (x), \
            addr(&timer_handler), \
            KERNEL_CS, \
            GDT_FLAGS_TYPE_32TRAP_GATE);

    #define __exception(x, nr, ...) \
        extern void exc_##x##_handler(); \
        idt_set_gate( \
            nr, \
            addr(&exc_##x##_handler), \
            KERNEL_CS, \
            GDT_FLAGS_TYPE_32TRAP_GATE);

    #define __exception_debug __exception

    #include <arch/isr.h>
    #include <arch/exception.h>

    idt_load(&idt.idt);
}
