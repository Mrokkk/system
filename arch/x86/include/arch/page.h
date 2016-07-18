#ifndef ARCH_X86_INCLUDE_ARCH_PAGE_H_
#define ARCH_X86_INCLUDE_ARCH_PAGE_H_

#ifndef __ASSEMBLER__

#define PGD_PRESENT     (1 << 0)
#define PGD_WRITEABLE   (1 << 1)
#define PGD_USER        (1 << 2)
#define PGD_WRITETHR    (1 << 3)
#define PGD_CACHEDIS    (1 << 4)
#define PGD_ACCESSED    (1 << 5)
#define PGD_RESERVED    (1 << 6)
#define PGD_SIZE        (1 << 7)

#define PGT_PRESENT     (1 << 0)
#define PGT_WRITEABLE   (1 << 1)
#define PGT_USER        (1 << 2)
#define PGT_WRITETHR    (1 << 3)
#define PGT_CACHEDIS    (1 << 4)
#define PGT_ACCESSED    (1 << 5)
#define PGT_DIRTY       (1 << 6)
#define PGT_RESERVED    (1 << 7)
#define PGT_GLOBAL      (1 << 8)

typedef unsigned int pgd_t;
typedef unsigned int pgt_t;

void *page_alloc();
int page_free(void *address);

static inline void page_directory_load(pgd_t *pgd) {
    asm volatile(
            "mov %0, %%cr3;"
            "mov $1f, %0;"
            "jmp *%0;"
            "1:"
            :: "r" (pgd) : "memory");
}

static inline void page_directory_reload() {
    register int dummy = 0;
    asm volatile(
            "mov %%cr3, %0;"
            "mov %0, %%cr3;"
            : "=r" (dummy)
            : "r" (dummy)
            : "memory"
    );
}

static inline void invlpg(void *address) {
    asm volatile(
            "invlpg (%0);"
            :: "r" (address)
            : "memory"
    );
}

#endif

#define PAGE_SIZE 0x1000
#define PAGE_NUMBER 0x100000
#define PAGE_TABLES_NUMBER 1024
#define PAGE_INIT_FLAGS (0x7)
#define KERNEL_PAGE_OFFSET (0xc0000000)

#define phys_address(virt) \
    ((typeof(virt))((unsigned int)virt - KERNEL_PAGE_OFFSET))

#define virt_address(phys) \
    ((typeof(phys))((unsigned int)phys + KERNEL_PAGE_OFFSET))

#endif /* ARCH_X86_INCLUDE_ARCH_PAGE_H_ */
