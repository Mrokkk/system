#pragma once

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <common/list.h>
#include <kernel/api/elf.h>

#include "loader.h"

typedef struct lib lib_t;
typedef struct rel rel_t;
typedef struct auxv auxv_t;
typedef struct hash hash_t;
typedef struct strtab strtab_t;
typedef struct symbol symbol_t;
typedef struct symtab symtab_t;
typedef struct dynamic dynamic_t;

struct auxv
{
    int         _AT_EXECFD;
    uintptr_t   _AT_PHDR;
    uintptr_t   _AT_ENTRY;
    uintptr_t   _AT_PHNUM;
    uintptr_t   _AT_PAGESZ;
    uintptr_t   _AT_BASE;
    const char* _AT_EXECFN;
};

struct strtab
{
    uint32_t    offset;
    const char* strings;
    size_t      size;
};

struct symtab
{
    size_t       entsize;
    uintptr_t    offset;
    elf32_sym_t* entries;
    size_t       count;
};

struct rel
{
    size_t       count;
    uintptr_t    offset;
    size_t       size;
    size_t       entsize;
    elf32_rel_t* entries;
};

struct hash
{
    size_t     offset;
    size_t     size;
    uintptr_t* data;
};

struct dynamic
{
    size_t       count;
    size_t       size;
    elf32_dyn_t* entries;

    hash_t       hash;
    rel_t        rel;
    rel_t        jmprel;
    symtab_t     symtab;
    strtab_t     dynstr;
    list_head_t  libraries;
};

struct symbol
{
    const char*  name;
    int          type;
    elf32_sym_t* symbol;
    elf32_rel_t* rel;
    uintptr_t    base_address;
    list_head_t  missing;
};

struct lib
{
    const char* name;
    uintptr_t   string_ndx;
    list_head_t list_entry;
};

#define FOR_EACH(x, ...) \
    do \
    { \
        typeof((x).entries) entry = (x).entries; \
        for (size_t i = 0; i < (x).count; entry = &(x).entries[++i]) \
        { \
            __VA_ARGS__; \
        } \
    } \
    while (0)

#define DYNSTR(dynamic, str) \
    ({ (dynamic)->dynstr.strings + str; })

#define SYMBOL(dynamic, entry) \
    ({ &(dynamic)->symtab.entries[ELF32_R_SYM((entry)->r_info)]; })

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

#define SYMTAB_DECLARE(name) \
    symtab_t name = { \
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
    if (LIKELY(!debug))
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

static inline uint32_t elf_hash(const char* name)
{
    uint32_t h = 0, g;

    while (*name)
    {
        h = (h << 4) + *((uint8_t*)name++);
        if ((g = h & 0xf0000000))
        {
            h ^= g >> 24;
        }
        h &= ~g;
    }
    return h;
}

const char* strtab_read(strtab_t* this, uint32_t addr);

void mmap_phdr(int exec_fd, size_t page_mask, elf32_phdr_t* phdr, uintptr_t base);
void mprotect_phdr(uintptr_t base_address, size_t page_size, int additional, elf32_phdr_t* phdr);
void mimmutable_phdr(uintptr_t base_address, size_t page_size, elf32_phdr_t* phdr);

#define PHDR_FOR_EACH(name, phdr, phnum) \
    for (elf32_phdr_t* name = phdr; name != (phdr) + (phnum); ++name)
