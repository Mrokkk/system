#include <kernel/memory.h>
#include <kernel/sections.h>

uint32_t ram;
uint64_t full_ram;
memory_area_t memory_areas[MEMORY_AREAS_SIZE];
static memory_area_t* current = memory_areas;

void memory_area_add(uint64_t start, uint64_t end, int type)
{
    current->start = start;
    current->end = end;
    current->type = type;

    if (type == MMAP_TYPE_AVL)
    {
        if (end - 1 <= (uint64_t)~0UL)
        {
            ram = end;
        }
        full_ram += end - start;
    }

    ++current;
}
