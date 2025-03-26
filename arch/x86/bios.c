#include <arch/bios.h>

void* bios_find(uint32_t signature)
{
    uint32_t* ebda = bios_ebda_get();
    uint32_t* ebda_end = ebda + 0x400;
    uint32_t* bios = ptr(0xe0000);
    uint32_t* bios_end = ptr(0x100000);

    for (; ebda < ebda_end; ebda += 4)
    {
        if (*ebda == signature)
        {
            return ptr(ebda);
        }
    }

    for (; bios < bios_end; bios += 4)
    {
        if (*bios == signature)
        {
            return ptr(bios);
        }
    }

    return NULL;
}
