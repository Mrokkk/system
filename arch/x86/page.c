#include <kernel/kernel.h>
#include <kernel/mutex.h>
#include <arch/page.h>
#include <arch/descriptor.h>
#include <arch/segment.h>
#include <arch/io.h>

extern pgd_t page_dir[];
extern pgt_t page0[];

unsigned int frames[32768];

pgt_t *page_table;
pgd_t *kernel_page_dir;

MUTEX_DECLARE(page_mutex);

static inline void page_set(int nr, unsigned int val) {
    page_table[nr] = val;
}

static inline void page_table_set(int nr, unsigned int val) {
    kernel_page_dir[nr] = val;
}

static inline int frame_is_free(unsigned int addr) {

    unsigned int frame = addr / 4096;
    return !(frames[frame / 32] & (1 << (frame % 32)));

}

static inline void frame_alloc(unsigned int i) {

    frames[i / 32] |= (1 << (i % 32));

}

void *page_alloc() {

    unsigned int i, end = (unsigned int)_end - KERNEL_PAGE_OFFSET;
    unsigned int frame_nr, address;

    mutex_lock(&page_mutex);

    for (i = end / PAGE_SIZE; i < ram / PAGE_SIZE; i++)
        if (frame_is_free(i * PAGE_SIZE)) break;

    frame_alloc(i);
    frame_nr = i;
    address = frame_nr * 4096;
    page_set(i, address | PGT_PRESENT | PGT_WRITEABLE | PGT_USER);

    mutex_unlock(&page_mutex);

    ASSERT(address >= end);
    ASSERT(page_table[address / PAGE_SIZE]);

    return (void *)virt_address(frame_nr * PAGE_SIZE);

}

int page_free(void *address) {

    unsigned int frame;

    mutex_lock(&page_mutex);

    frame = ((unsigned int)address - KERNEL_PAGE_OFFSET) / PAGE_SIZE;

    ASSERT(!frame_is_free(frame));

    frames[frame / 32] &= ~(1 << (frame % 32));
    page_set(frame, 0);

    invlpg(address);

    mutex_unlock(&page_mutex);

    return 0;

}

int paging_init() {

    unsigned int end = (unsigned int)_end - KERNEL_PAGE_OFFSET,
                 frame_count = end / PAGE_SIZE,
                 count = frame_count /  32,
                 bits = frame_count % 32,
                 i, j;
    pgt_t *temp_pgt;

    kernel_page_dir = virt_address((pgd_t *)page_dir);
    temp_pgt = page_table = virt_address((pgt_t *)page0);

    for (i = 0; i < count; i++)
        frames[i] = ~0UL;

    frames[count] = (~0UL >> (32 - bits));

    page_table = page_alloc();
    memcpy(page_table, temp_pgt, PAGE_SIZE);

    for (i = 768, j = 0; i < PAGE_TABLES_NUMBER; i++, j+=1024)
        page_table_set(i, phys_address((unsigned int)&page_table[j]) |
                PGD_PRESENT | PGD_WRITEABLE | PGD_USER);

    for (i = 1; i < 1024; i++)
        end = (unsigned int)page_alloc();

    page_directory_reload();

    ASSERT(!frame_is_free(phys_address(end)));
    ASSERT(frame_is_free(phys_address(end) + PAGE_SIZE));
    ASSERT(!frame_is_free(phys_address(end) - PAGE_SIZE));
    ASSERT(!frame_is_free(phys_address(end) - phys_address(end) / 2));
    ASSERT(!frame_is_free(PAGE_SIZE));
    ASSERT(!frame_is_free(0x0));
    ASSERT(page_table[phys_address(end) / PAGE_SIZE]);

    return 0;

}

