#include <kernel/memory.h>

uint32_t ram;
memory_area_t memory_areas[MEMORY_AREAS_SIZE];
static memory_area_t* current = memory_areas;

void memory_area_add(uint32_t base, uint32_t size, int type)
{
    current->base = base;
    current->size = size;
    current->type = type;

    if (type == MMAP_TYPE_AVL)
    {
        ram = base + size;
    }

    ++current;
}
