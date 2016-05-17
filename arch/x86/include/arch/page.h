#ifndef ARCH_X86_INCLUDE_ARCH_PAGE_H_
#define ARCH_X86_INCLUDE_ARCH_PAGE_H_

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
    asm volatile("mov %0, %%cr3;" :: "r" (pgd) : "memory");
}

#endif /* ARCH_X86_INCLUDE_ARCH_PAGE_H_ */
