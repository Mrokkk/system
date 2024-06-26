#pragma once

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <kernel/api/elf.h>

#include "loader.h"

typedef struct rel rel_t;
typedef struct auxv auxv_t;
typedef struct strtab strtab_t;
typedef struct symtab symtab_t;
typedef struct dynamic dynamic_t;

struct auxv
{
    int _AT_EXECFD;
    uintptr_t _AT_PHDR;
    uintptr_t _AT_ENTRY;
    uintptr_t _AT_PHNUM;
    uintptr_t _AT_PAGESZ;
    uintptr_t _AT_BASE;
};

struct strtab
{
    const char* (*read)(strtab_t* this, uint32_t addr);

    uint32_t offset;
    const char* strings;
    size_t size;
};

struct symtab
{
    elf32_sym* symbols;
    size_t count;
};

struct rel
{
    uintptr_t offset;
    size_t size;
    size_t entsize;
    elf32_rel_t* entries;
};

struct dynamic
{
    size_t count;
    size_t size;
    elf32_dyn_t* entries;
};

#define STRTAB_DECLARE(name) \
    strtab_t name = { \
        .read = &strtab_read \
    }

#define REL_DECLARE(name) \
    rel_t name = { \
    }

#define DYNAMIC_DECLARE(name) \
    dynamic_t name = { \
    }

#define PHDR_PRINT(phdr) \
    do \
    { \
        printf("%8s %#010x %#010x %#010x %#010x %#010x %#010x %#010x\n", \
            elf_phdr_type_get((phdr)->p_type), \
            (phdr)->p_offset, \
            (phdr)->p_vaddr, \
            (phdr)->p_paddr, \
            (phdr)->p_filesz, \
            (phdr)->p_memsz, \
            (phdr)->p_flags, \
            (phdr)->p_align); \
    } while (0)

static inline char* elf_phdr_type_get(uint32_t type)
{
    switch (type)
    {
        case PT_NULL: return "NULL";
        case PT_LOAD: return "LOAD";
        case PT_DYNAMIC: return "DYNAMIC";
        case PT_INTERP: return "INTERP";
        case PT_NOTE: return "NOTE";
        case PT_SHLIB: return "SHLIB";
        case PT_PHDR: return "PHDR";
        default: return "UNKNOWN";
    }
}

static inline void phdr_print(elf32_phdr_t* phdr, size_t phnum)
{
    if (!debug)
    {
        return;
    }

    printf("%8s %10s %10s %10s %10s %10s %10s %10s\n",
        "type", "offset", "vaddr", "paddr", "filesz", "memsz", "flags", "align");

    for (size_t i = 0; i < phnum; ++i)
    {
        PHDR_PRINT(&phdr[i]);
    }
}

const char* strtab_read(strtab_t* this, uint32_t addr);

void* mmap_phdr(int exec_fd, size_t page_mask, elf32_phdr_t* phdr);
void mprotect_phdr(uintptr_t base_address, size_t page_size, int additional, elf32_phdr_t* phdr);
