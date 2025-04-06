#pragma once

#include <kernel/compiler.h>
#include <kernel/page_types.h>

typedef enum
{
    PAGE_ALLOC_DISCONT      = 0,
    PAGE_ALLOC_CONT         = 1,
    PAGE_ALLOC_UNCACHED     = 2,
    PAGE_ALLOC_NO_KERNEL    = 4,
    PAGE_ALLOC_ZEROED       = 8,
} alloc_flag_t;

// Allocate a page(s) and map it/them in kernel; allocation of
// multiple pages is done according to PAGE_ALLOC_* flags.
// Returned pages are linked together through list_entry (first
// page is list's head)
MUST_CHECK(page_t*) __page_alloc(int count, alloc_flag_t flag);
MUST_CHECK(page_t*) pages_split(page_t* pages, size_t pages_to_keep);
void pages_merge(page_t* new_pages, page_t* pages);
void __pages_free(page_t* pages);

void page_kernel_map(page_t* page, pgprot_t prot);
void page_kernel_unmap(page_t* page);

#define pages_free(ptr)         __pages_free(ptr)
#define page_alloc(c, f)        __page_alloc(c, f)

#define single_page() \
    ({ page_t* res = page_alloc(1, PAGE_ALLOC_DISCONT); res ? page_virt_ptr(res) : NULL; })

#define PAGES_FOR_EACH(page, pages) \
    bool __started; \
    for (page = pages, __started = false; \
        !__started || page != (pages); \
        page = list_next_entry(&page->list_entry, page_t, list_entry), __started = true)
