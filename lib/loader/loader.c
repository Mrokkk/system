#include "loader.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <common/list.h>
#include <kernel/api/elf.h>

#include "elf.h"

// References:
// https://docs.oracle.com/cd/E18752_01/html/817-1984/chapter6-14428.html
// https://flapenguin.me/elf-dt-hash
// https://www.sco.com/developers/devspecs/abi386-4.pdf

int debug;
static auxv_t saved_auxv = {
    ._AT_EXECFD = -1
};
static LIST_DECLARE(libs);

#define STORE(what, where) \
    case what: where = value; break

#define AUX_STORE(a) \
    case a: \
        saved_auxv._##a = (typeof(saved_auxv._##a))aux[i].a_un.a_val; break

#define AUX_VERIFY(a, bad_value) \
    do { if (UNLIKELY(saved_auxv._##a == bad_value)) die("missing " #a); } while (0)

#define AUX_GET(a) \
    ({ saved_auxv._##a; })

static __attribute__((noinline)) void loader_breakpoint(const char* name, uintptr_t base_address)
{
    asm volatile("" :: "a" (name), "c" (base_address) : "memory");
}

static void auxv_read(elf32_auxv_t** vec)
{
    elf32_auxv_t* aux = *vec;

    for (size_t i = 0; aux[i].a_type; ++i)
    {
        switch (aux[i].a_type)
        {
            AUX_STORE(AT_EXECFD);
            AUX_STORE(AT_PHDR);
            AUX_STORE(AT_PAGESZ);
            AUX_STORE(AT_PHNUM);
            AUX_STORE(AT_ENTRY);
            AUX_STORE(AT_EXECFN);
            AUX_STORE(AT_BASE);
        }
    }

    AUX_VERIFY(AT_EXECFD, -1);
    AUX_VERIFY(AT_PHDR, 0);
    AUX_VERIFY(AT_PAGESZ, 0);
    AUX_VERIFY(AT_PHNUM, 0);
    AUX_VERIFY(AT_ENTRY, 0);
    AUX_VERIFY(AT_BASE, 0);
    AUX_VERIFY(AT_EXECFN, 0);
}

static void dynamic_store(dynamic_t* dynamic, list_head_t* libs)
{
    FOR_EACH(*dynamic,
    {
        uint32_t value = entry->d_un.d_val;
        switch (entry->d_tag)
        {
            case DT_NULL: return;
            case DT_NEEDED:
            {
                lib_t* lib = ALLOC(malloc(sizeof(*lib)));
                list_init(&lib->list_entry);
                lib->string_ndx = value;
                list_add_tail(&lib->list_entry, libs);
                break;
            }
            STORE(DT_REL, dynamic->rel.offset);
            STORE(DT_RELSZ, dynamic->rel.size);
            STORE(DT_RELENT, dynamic->rel.entsize);
            STORE(DT_STRSZ, dynamic->dynstr.size);
            STORE(DT_STRTAB, dynamic->dynstr.offset);
            STORE(DT_SYMTAB, dynamic->symtab.offset);
            STORE(DT_SYMENT, dynamic->symtab.entsize);
            STORE(DT_JMPREL, dynamic->jmprel.offset);
            STORE(DT_PLTRELSZ, dynamic->jmprel.size);
            STORE(DT_HASH, dynamic->hash.offset);
        }
    });
}

static elf32_sym_t* elf_lookup(dynamic_t* dynamic, const char* symname)
{
    uint32_t hash = elf_hash(symname);

    uint32_t* hashtab = dynamic->hash.data;
    elf32_sym_t* symtab = dynamic->symtab.entries;
    const char* strtab = dynamic->dynstr.strings;

    uint32_t nbucket = hashtab[0];
    uint32_t nchain = hashtab[1];
    uint32_t* bucket = &hashtab[2];
    uint32_t* chain = &bucket[nbucket];

    UNUSED(nchain);

    for (uint32_t i = bucket[hash % nbucket]; i; i = chain[i])
    {
        if (!strcmp(symname, strtab + symtab[i].st_name))
        {
            return &symtab[i];
        }
    }

    return NULL;
}

static void dynamic_read(elf32_phdr_t* p, dynamic_t* dynamic, list_head_t* libs, uintptr_t base)
{
    dynamic->size = p->p_filesz;
    dynamic->entries = SHIFT_AS(elf32_dyn_t*, base, p->p_vaddr);
    dynamic->count = dynamic->size / sizeof(elf32_dyn_t);

    dynamic_store(dynamic, libs);

    dynamic->symtab.count = (dynamic->dynstr.offset - dynamic->symtab.offset) / dynamic->symtab.entsize;
    dynamic->symtab.entries = SHIFT_AS(elf32_sym_t*, base, dynamic->symtab.offset);
    dynamic->dynstr.strings = SHIFT_AS(const char*, base, dynamic->dynstr.offset);
    dynamic->rel.entries = SHIFT_AS(elf32_rel_t*, base, dynamic->rel.offset);
    dynamic->rel.count = dynamic->rel.size / dynamic->rel.entsize;
    dynamic->hash.size =  dynamic->symtab.offset - dynamic->hash.offset;
    dynamic->hash.data = SHIFT_AS(uintptr_t*, base, dynamic->hash.offset);

    if (dynamic->jmprel.offset)
    {
        dynamic->jmprel.entries = SHIFT_AS(elf32_rel_t*, base, dynamic->jmprel.offset);
        dynamic->jmprel.count = dynamic->jmprel.size / sizeof(elf32_rel_t);
    }
}

DIAG_IGNORE("-Wanalyzer-malloc-leak");

static void missing_symbol_add(const char* name, elf32_sym_t* symbol, elf32_rel_t* rel, int type, uintptr_t base_address, list_head_t* missing_symbols)
{
    symbol_t* missing = ALLOC(malloc(sizeof(*missing)));

    DEBUG("adding missing symbol %s\n", name);

    list_init(&missing->missing);
    missing->name = name;
    missing->base_address = base_address;
    missing->type = type;
    missing->symbol = symbol;
    missing->rel = rel;
    list_add_tail(&missing->missing, missing_symbols);
}

DIAG_RESTORE();

#define ENSURE(x, ...) \
    if (UNLIKELY(!(x))) \
    { \
        fprintf(stderr, __VA_ARGS__); \
        die("Cannot load executable"); \
    }

static void symbol_relocate(
    const elf32_sym_t* symbol,
    const elf32_rel_t* rel,
    uintptr_t base_address, // base address of the binary where symbol is needed
    uintptr_t lib_base_address)
{
    int type;
    uintptr_t* memory = PTR(base_address + rel->r_offset);

    switch (type = ELF32_R_TYPE(rel->r_info))
    {
        // A - The addend used to compute the value of the relocatable field.
        // B - The base address at which a shared object is loaded into memory during
        //     execution. Generally, a shared object file is built with a base virtual
        //     address of 0. However, the execution address of the shared object is
        //     different.
        // S - The value of the symbol whose index resides in the relocation entry.
        // P - The section offset or address of the storage unit being relocated, computed using r_offset.
        case R_386_RELATIVE: // B + A
        {
            uintptr_t B = lib_base_address;
            uintptr_t A = *memory;
            *memory = B + A;
            break;
        }

        case R_386_GLOB_DAT:
        case R_386_JMP_SLOT: // S
        {
            ENSURE(symbol, "missing symbol for %#x relocation at %p\n", type, memory);
            uintptr_t S = lib_base_address + symbol->st_value;
            *memory = S;
            break;
        }

        case R_386_32: // S + A
        {
            ENSURE(symbol, "missing symbol for %#x relocation at %p\n", type, memory);
            uintptr_t S = lib_base_address + symbol->st_value;
            uintptr_t A = *memory;

            *memory = S + A;
            break;
        }

        case R_386_PC32: // S + A - P
        {
            ENSURE(symbol, "missing symbol for R_386_PC32 relocation at %p\n", memory);
            uintptr_t S = lib_base_address + symbol->st_value;
            uintptr_t A = *memory;
            uintptr_t P = ADDR(memory);

            *memory = S + A - P;
            break;
        }

        default:
        {
            fprintf(stderr, "warning: unsupported rel type: %#x\n", type);
        }
    }
}

static void relocate(rel_t* rel, dynamic_t* dynamic, uintptr_t base_address, list_head_t* missing_symbols)
{
    FOR_EACH(*rel,
    {
        elf32_sym_t* symbol = NULL;
        int type = ELF32_R_TYPE(entry->r_info);

        switch (type)
        {
            case R_386_32:
            case R_386_PC32:
            case R_386_GLOB_DAT:
            case R_386_JMP_SLOT:
            {
                symbol = SYMBOL(dynamic, entry);

                if (!symbol->st_shndx)
                {
                    missing_symbol_add(
                        DYNSTR(dynamic, symbol->st_name),
                        symbol,
                        entry,
                        type,
                        base_address,
                        missing_symbols);
                    continue;
                }

                FALLTHROUGH;
            }

            default:
            {
                symbol_relocate(symbol, entry, base_address, base_address);
            }
        }
    });
}

static void missing_symbols_verify(list_head_t* missing_symbols)
{
    if (!list_empty(missing_symbols))
    {
        symbol_t* s;
        fprintf(stderr, "%s: cannot load executable\n", AUX_GET(AT_EXECFN));
        list_for_each_entry(s, missing_symbols, missing)
        {
            fprintf(stderr, "  missing symbol %s\n", s->name);
        }
        exit(EXIT_FAILURE);
    }
}

static void link(dynamic_t* dynamic, int, uintptr_t base_address, uintptr_t lib_base, uintptr_t* brk_address)
{
    LIST_DECLARE(missing_symbols);

    relocate(&dynamic->rel, dynamic, base_address, &missing_symbols);
    relocate(&dynamic->jmprel, dynamic, base_address, &missing_symbols);

    lib_t* lib;
    list_for_each_entry(lib, &libs, list_entry)
    {
        int lib_fd;
        char path[128];
        elf32_phdr_t* phdr;
        elf32_header_t* header;
        DYNAMIC_DECLARE(lib_dynamic);
        LIST_DECLARE(lib_libs);

        sprintf(path, "/usr/lib/%s", DYNSTR(dynamic, lib->string_ndx));

        lib_fd = open(path, O_RDONLY);

        if (UNLIKELY(lib_fd == -1))
        {
            perror(path);
            exit(EXIT_FAILURE);
        }

        loader_breakpoint(path, lib_base);

        header = alloc_read(lib_fd, sizeof(*header), 0);
        phdr = alloc_read(lib_fd, header->e_phentsize * header->e_phnum, header->e_phoff);

        phdr_print(phdr, header->e_phnum);

        for (size_t i = 0; i < header->e_phnum; ++i)
        {
            elf32_phdr_t* p = &phdr[i];

            switch (p->p_type)
            {
                case PT_LOAD:
                {
                    *brk_address = ALIGN_TO(p->p_memsz + p->p_vaddr + lib_base, AUX_GET(AT_PAGESZ));
                    mmap_phdr(lib_fd, AUX_GET(AT_PAGESZ), p, p->p_flags & PF_X ? PF_W : 0, lib_base);
                    break;
                }

                case PT_DYNAMIC:
                {
                    dynamic_read(p, &lib_dynamic, &lib_libs, lib_base);

                    relocate(&lib_dynamic.rel, &lib_dynamic, lib_base, &missing_symbols);
                    relocate(&lib_dynamic.jmprel, &lib_dynamic, lib_base, &missing_symbols);

                    symbol_t* s;
                    list_for_each_entry_safe(s, &missing_symbols, missing)
                    {
                        const elf32_sym_t* symbol = elf_lookup(&lib_dynamic, s->name);
                        if (symbol && symbol->st_shndx)
                        {
                            symbol_relocate(symbol, s->rel, s->base_address, lib_base);
                            DEBUG("resolved symbol: %s\n", s->name);
                            list_del(&s->missing);
                        }
                    }
                    break;
                }
            }
        }

        for (size_t i = 0; i < header->e_phnum; ++i)
        {
            elf32_phdr_t* p = &phdr[i];

            switch (p->p_type)
            {
                case PT_LOAD:
                    if (p->p_flags & PF_X)
                    {
                        mprotect_phdr(lib_base, AUX_GET(AT_PAGESZ), 0, p);
                    }
                    lib_base = ALIGN_TO(p->p_memsz + p->p_vaddr + lib_base, AUX_GET(AT_PAGESZ));
                    break;
            }
        }

        close(lib_fd);
    }

    missing_symbols_verify(&missing_symbols);
}

static __attribute__((noreturn)) void loader_main(elf32_auxv_t** auxv, void* stack_ptr)
{
    int exec_fd = 0;
    elf32_phdr_t* phdr = NULL;
    uintptr_t brk_address = 0;
    uintptr_t base_address = 0;
    uintptr_t lib_base = 0;
    DYNAMIC_DECLARE(dynamic);

    debug = !!getenv("L");

    auxv_read(auxv);

    exec_fd = AUX_GET(AT_EXECFD);
    phdr = PTR(AUX_GET(AT_PHDR));
    base_address = AUX_GET(AT_BASE);

    phdr_print(phdr, AUX_GET(AT_PHNUM));

    loader_breakpoint(AUX_GET(AT_EXECFN), base_address);

    for (size_t i = 0; i < AUX_GET(AT_PHNUM); ++i)
    {
        elf32_phdr_t* p = &phdr[i];

        switch (p->p_type)
        {
            case PT_DYNAMIC:
            {
                dynamic_read(p, &dynamic, &libs, base_address);
                break;
            }

            case PT_LOAD:
            {
                brk_address = ALIGN_TO(p->p_memsz + p->p_vaddr + base_address, AUX_GET(AT_PAGESZ));

                // Allow writing over segment to perform relocations
                if (phdr[i].p_flags & PF_X)
                {
                    mprotect_phdr(base_address, AUX_GET(AT_PAGESZ), PF_W, p);
                }
                lib_base = ALIGN_TO(p->p_memsz + p->p_vaddr + base_address, AUX_GET(AT_PAGESZ));
                break;
            }
        }
    }

    link(&dynamic, exec_fd, AUX_GET(AT_BASE), lib_base, &brk_address);

    for (size_t i = 0; i < AUX_GET(AT_PHNUM); ++i)
    {
        // Restore original protection flags
        if (phdr[i].p_type == PT_LOAD && phdr[i].p_flags & PF_X)
        {
            mprotect_phdr(base_address, AUX_GET(AT_PAGESZ), 0, &phdr[i]);
        }
    }

    close(exec_fd);
    brk((void*)brk_address);

    // Load original stack ptr so the executable will
    // see argc, argv, envp as was setup by kernel
    // and jump to the entry
    asm volatile(
        "mov $0, %%ebp;"
        "mov %1, %%esp;"
        "jmp *%0"
        :: "r" (AUX_GET(AT_ENTRY)), "r" (stack_ptr)
        : "memory");

    while (1);
}

__attribute__((noreturn)) int main(int argc, char*[], char* envp[])
{
    loader_main(SHIFT_AS(elf32_auxv_t**, &envp, 4), &argc);
}

__attribute__((naked,noreturn,used)) int _start()
{
    asm volatile(
        "call __libc_start_main;"
        "call main;");
}
