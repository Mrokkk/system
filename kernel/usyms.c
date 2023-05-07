#include <kernel/usyms.h>
#include <kernel/kernel.h>

usym_t* usym_find(uint32_t address, usym_t* symbols)
{
    usym_t* sym = symbols;
    for (; sym; sym = sym->next)
    {
        if (address >= sym->start && address < sym->end)
        {
            return sym;
        }
    }

    return NULL;
}

void usym_string(char* buffer, uint32_t addr, usym_t* symbols)
{
    usym_t* symbol = usym_find(addr, symbols);

    if (symbol)
    {
        sprintf(buffer, "[<%08x>] %s+%x/%x",
            addr,
            symbol->name,
            addr - symbol->start,
            symbol->end - symbol->start);
    }
    else
    {
        sprintf(buffer, "%x <unknown>", addr);
    }
}
