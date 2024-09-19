#include <kernel/page.h>
#include <kernel/kernel.h>
#include <kernel/sections.h>

#define FREE_SECTION 7

#define SECTION_DEF(name, flags) \
    {#name, _##name##_start, _##name##_end, flags}

section_t sections[] = {
    SECTION_DEF(unpaged_transient, SECTION_READ | SECTION_WRITE | SECTION_UNPAGED | SECTION_UNMAP_AFTER_INIT),
    SECTION_DEF(unpaged_eternal, SECTION_READ | SECTION_WRITE | SECTION_UNPAGED),
    SECTION_DEF(text_init, SECTION_READ | SECTION_EXEC | SECTION_UNMAP_AFTER_INIT),
    SECTION_DEF(text, SECTION_READ | SECTION_EXEC),
    SECTION_DEF(rodata, SECTION_READ),
    SECTION_DEF(data, SECTION_READ | SECTION_WRITE),
    SECTION_DEF(bss, SECTION_READ | SECTION_WRITE),
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0}
};

int section_add(const char* name, void* start, void* end, int flags)
{
    if (sections[FREE_SECTION].name)
    {
        return -EBUSY;
    }

    sections[FREE_SECTION].name = name;
    sections[FREE_SECTION].start = start;
    sections[FREE_SECTION].end = end;
    sections[FREE_SECTION].flags = flags;

    return 0;
}

void section_free(section_t* section)
{
    uintptr_t start, end;
    if (section->flags & SECTION_UNPAGED)
    {
        start = virt_addr(section->start);
        end = virt_addr(section->end);
    }
    else
    {
        start = addr(section->start);
        end = addr(section->end);
    }

    log_notice("freeing [%08x - %08x] %s", section->start, section->end, section->name);

    for (uintptr_t vaddr = start; vaddr < end; vaddr += PAGE_SIZE)
    {
        page_free(ptr(vaddr));
    }
}
