#include <kernel/kernel.h>
#include <kernel/sections.h>
#include <kernel/page_alloc.h>

#define FREE_SECTION 8

#define SECTION_DEF(name, flags) \
    {#name, _##name##_start, _##name##_end, flags}

section_t sections[] = {
    SECTION_DEF(unpaged_transient, SECTION_READ | SECTION_WRITE | SECTION_UNPAGED | SECTION_UNMAP_AFTER_INIT),
    SECTION_DEF(text_init, SECTION_READ | SECTION_EXEC | SECTION_UNMAP_AFTER_INIT),
    SECTION_DEF(text, SECTION_READ | SECTION_EXEC),
    SECTION_DEF(rodata_after_init, SECTION_READ | SECTION_WRITE),
    SECTION_DEF(rodata, SECTION_READ),
    SECTION_DEF(data, SECTION_READ | SECTION_WRITE),
    SECTION_DEF(data_per_cpu, SECTION_READ | SECTION_WRITE),
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

uintptr_t section_phys_start(const section_t* section)
{
    return section->flags & SECTION_UNPAGED
        ? addr(section->start)
        : phys_addr(section->start);
}

uintptr_t section_phys_end(const section_t* section)
{
    return section->flags & SECTION_UNPAGED
        ? addr(section->end)
        : phys_addr(section->end);
}
