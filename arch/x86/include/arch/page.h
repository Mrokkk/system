#ifndef ARCH_X86_INCLUDE_ARCH_PAGE_H_
#define ARCH_X86_INCLUDE_ARCH_PAGE_H_

#ifndef __ASSEMBLER__

struct page_directory {
    unsigned int present:1;
    unsigned int writeable:1;
    unsigned int user_access:1;
    unsigned int write_through:1;
    unsigned int cache_disabled:1;
    unsigned int accessed:1;
    unsigned int reserved:1;
    unsigned int size:1;
    unsigned int global_page:1;
    unsigned int avail:3;
    unsigned int address:20;
} __attribute__((packed));

struct page_table {
    unsigned int present:1;
    unsigned int writeable:1;
    unsigned int user_access:1;
    unsigned int write_through:1;
    unsigned int cache_disabled:1;
    unsigned int accessed:1;
    unsigned int dirty:1;
    unsigned int reserved:1;
    unsigned int global_page:1;
    unsigned int avail:3;
    unsigned int address:20;
} __attribute__((packed));

extern inline void page_directory_load(struct page_directory *pgd) {
    asm volatile(
            "mov %0, %%cr3;"
            "mov $1f, %0;"
            "jmp *%0;"
            "1:"
            :: "r" (pgd) : "memory");
}

static inline void tlb_flush() {
    register int dummy = 0;
    asm volatile(
            "mov %%cr3, %0;"
            "mov %0, %%cr3;"
            : "=r" (dummy)
            : "r" (dummy)
            : "memory"
    );
}

#endif

#define PAGE_INIT_FLAGS (0x7)
#define KERNEL_PAGE_OFFSET (0xc0000000)

#define phys_address(virt) \
    ((typeof(virt))((unsigned int)virt - KERNEL_PAGE_OFFSET))

#define virt_address(phys) \
    ((typeof(phys))((unsigned int)phys + KERNEL_PAGE_OFFSET))

#endif /* ARCH_X86_INCLUDE_ARCH_PAGE_H_ */
