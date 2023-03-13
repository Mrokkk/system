#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel/kernel.h>

#define SYMBOL_NAME_SIZE 32

typedef struct
{
    char name[SYMBOL_NAME_SIZE];
    uint32_t address;
    uint32_t size;
    char type;
} ksym_t;

ksym_t* ksym_find_by_name(const char *name);
ksym_t* ksym_find(uint32_t address);
int ksyms_load(char* symbols, unsigned int size);

static inline void ksym_string(char* buffer, uint32_t addr)
{
    ksym_t* symbol = ksym_find(addr);

    if (symbol)
    {
        sprintf(buffer, "%x <%s+%x>",
            addr,
            symbol->name,
            addr - symbol->address);
    }
    else
    {
        sprintf(buffer, "%x <unknown>", addr);
    }
}
