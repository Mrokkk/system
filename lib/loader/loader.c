#include "loader.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
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
static auxv_t saved_auxv = { ._AT_EXECFD = -1 };
static LIST_DECLARE(libs);
static void* syscalls_start;
static size_t syscalls_size;

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

    DEBUG("adding missing symbol %s", name);

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
        die(__VA_ARGS__); \
    }

static const char* rel_name(int type)
{
    switch (type)
    {
        case R_386_NONE:     return "R_386_NONE";
        case R_386_32:       return "R_386_32";
        case R_386_PC32:     return "R_386_PC32";
        case R_386_GOT32:    return "R_386_GOT32";
        case R_386_PLT32:    return "R_386_PLT32";
        case R_386_COPY:     return "R_386_COPY";
        case R_386_GLOB_DAT: return "R_386_GLOB_DAT";
        case R_386_JMP_SLOT: return "R_386_JMP_SLOT";
        case R_386_RELATIVE: return "R_386_RELATIVE";
        case R_386_GOTOFF:   return "R_386_GOTOFF";
        case R_386_GOTPC:    return "R_386_GOTPC";
        case R_386_32PLT:    return "R_386_32PLT";
        case R_386_GOT32X:   return "R_386_GOT32X";
        default:             return "unknown";
    }
}

static void symbol_relocate(
    const char* symbol_name,
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
            DEBUG("%s: %p = %p + %p", rel_name(type), memory, B, A);
            break;
        }

        case R_386_GLOB_DAT:
        case R_386_JMP_SLOT: // S
        {
            ENSURE(symbol, "missing symbol for %#x relocation at %p", type, memory);
            uintptr_t S = lib_base_address + symbol->st_value;
            *memory = S;
            DEBUG("%s: %p = %p (symbol: %s)", rel_name(type), memory, S, symbol_name);
            break;
        }

        case R_386_32: // S + A
        {
            ENSURE(symbol, "missing symbol for %#x relocation at %p", type, memory);
            uintptr_t S = lib_base_address + symbol->st_value;
            uintptr_t A = *memory;

            *memory = S + A;
            DEBUG("%s: %p = %p + %p (symbol: %s)", rel_name(type), memory, S, A, symbol_name);
            break;
        }

        case R_386_PC32: // S + A - P
        {
            ENSURE(symbol, "missing symbol for %#x relocation at %p", type, memory);
            uintptr_t S = lib_base_address + symbol->st_value;
            uintptr_t A = *memory;
            uintptr_t P = ADDR(memory);

            *memory = S + A - P;
            DEBUG("%s: %p = %p + %p - %p (symbol: %s)", rel_name(type), memory, S, A, P, symbol_name);
            break;
        }

        case R_386_NONE: // none
        case R_386_COPY:
        {
            DEBUG("%s (symbol: %s)", rel_name(type), symbol_name);
            break;
        }

        default:
        {
            die("unsupported rel type: %s (%#x)", rel_name(type), type);
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
                symbol_relocate(NULL, symbol, entry, base_address, base_address);
            }
        }
    });
}

static void missing_symbols_verify(list_head_t* missing_symbols)
{
    if (UNLIKELY(!list_empty(missing_symbols)))
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

static void link(dynamic_t* dynamic, uintptr_t base_address, uintptr_t lib_base, uintptr_t* brk_address)
{
    uintptr_t page_size = AUX_GET(AT_PAGESZ);
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
        uintptr_t next_lib_base = 0;
        DYNAMIC_DECLARE(lib_dynamic);
        LIST_DECLARE(lib_libs);

        sprintf(path, "/lib/%s", DYNSTR(dynamic, lib->string_ndx));

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

        PHDR_FOR_EACH(p, phdr, header->e_phnum)
        {
            switch (p->p_type)
            {
                case PT_LOAD:
                {
                    *brk_address = ALIGN_TO(p->p_memsz + p->p_vaddr + lib_base, page_size);
                    next_lib_base = ALIGN_TO(p->p_memsz + p->p_vaddr + lib_base, page_size);
                    void* addr = mmap_phdr(lib_fd, page_size, p, lib_base);

                    if ((p->p_flags & PF_X) && !strcmp("libc.so", DYNSTR(dynamic, lib->string_ndx)))
                    {
                        syscalls_start = addr;
                        syscalls_size = p->p_memsz + p->p_vaddr + lib_base - ADDR(addr);
                    }

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
                            symbol_relocate(s->name, symbol, s->rel, s->base_address, lib_base);
                            list_del(&s->missing);
                        }
                    }
                    break;
                }
            }
        }

        lib_base = next_lib_base;

        close(lib_fd);
    }

    missing_symbols_verify(&missing_symbols);
}

extern void __libc_start_main(int, char* const argv[], char* const envp[]);

#define EARLY_ENSURE(x) \
    if (UNLIKELY(!(x))) \
    { \
        __builtin_trap(); \
    }

void relocate_itself(elf32_auxv_t** auxv)
{
    elf32_auxv_t* aux = *auxv;
    uintptr_t base = 0;
    uintptr_t page_size = 0;

    for (size_t i = 0; aux[i].a_type; ++i)
    {
        if (aux[i].a_type == AT_BASE)
        {
            base = aux[i].a_un.a_val;
        }
        else if (aux[i].a_type == AT_PAGESZ)
        {
            page_size = aux[i].a_un.a_val;
        }
    }

    EARLY_ENSURE(base && page_size);

    elf32_header_t* header = PTR(base);
    elf32_phdr_t* phdr = SHIFT_AS(elf32_phdr_t*, header, header->e_phoff);
    elf32_dyn_t* dyn = NULL;

    PHDR_FOR_EACH(p, phdr, header->e_phnum)
    {
        if (p->p_type == PT_DYNAMIC)
        {
            dyn = SHIFT_AS(elf32_dyn_t*, header, p->p_vaddr);
            break;
        }
        else if (p->p_type == PT_LOAD)
        {
            mimmutable_phdr(base, page_size, p);
        }
    }

    EARLY_ENSURE(dyn);

    uintptr_t rel_off = 0;
    size_t rel_size = 0;

    for (; dyn->d_tag != DT_NULL; ++dyn)
    {
        switch (dyn->d_tag)
        {
            case DT_REL:
                rel_off = dyn->d_un.d_off;
                break;
            case DT_RELSZ:
                rel_size = dyn->d_un.d_val;
                break;
        }
    }

    EARLY_ENSURE(rel_off && rel_size);

    elf32_rel_t* rel = SHIFT_AS(elf32_rel_t*, header, rel_off);
    elf32_rel_t* rel_end = SHIFT(rel, rel_size);

    for (; rel != rel_end; ++rel)
    {
        int type;
        uintptr_t* memory = PTR(base + rel->r_offset);
        switch (type = ELF32_R_TYPE(rel->r_info))
        {
            case R_386_RELATIVE: // B + A
            {
                uintptr_t B = base;
                uintptr_t A = *memory;
                *memory = B + A;
                break;
            }

            default:
                __builtin_trap();
        }
    }
}

static __attribute__((noreturn,noinline)) void loader_main(int argc, char* argv[], char* envp[], elf32_auxv_t** auxv, void* stack_ptr)
{
    int exec_fd = 0;
    elf32_phdr_t* phdr = NULL;
    uintptr_t brk_address = 0;
    uintptr_t base_address = 0;
    uintptr_t lib_base = 0;
    uintptr_t page_size = 0;
    DYNAMIC_DECLARE(dynamic);

    asm volatile("" ::: "memory");
    relocate_itself(auxv);
    asm volatile("" ::: "memory");

    __libc_start_main(argc, argv, envp);

    debug = !!getenv("L");

    print_init();

    auxv_read(auxv);

    exec_fd = AUX_GET(AT_EXECFD);
    phdr = PTR(AUX_GET(AT_PHDR));
    page_size = AUX_GET(AT_PAGESZ);
    base_address = AUX_GET(AT_PHDR) & ~(page_size - 1);

    phdr_print(phdr, AUX_GET(AT_PHNUM));

    PHDR_FOR_EACH(p, phdr, AUX_GET(AT_PHNUM))
    {
        switch (p->p_type)
        {
            case PT_PHDR:
            {
                // FIXME: hack for ET_EXEC built by tcc
                if (UNLIKELY((p->p_vaddr & ~(page_size - 1)) == base_address))
                {
                    base_address = 0;
                }
                break;
            }

            case PT_DYNAMIC:
            {
                dynamic_read(p, &dynamic, &libs, base_address);
                break;
            }

            case PT_LOAD:
            {
                mimmutable_phdr(base_address, page_size, p);
                brk_address = ALIGN_TO(p->p_memsz + p->p_vaddr + base_address, page_size);
                lib_base = ALIGN_TO(p->p_memsz + p->p_vaddr + base_address, page_size);
                break;
            }
        }
    }

    loader_breakpoint(AUX_GET(AT_EXECFN), base_address);

    link(&dynamic, base_address, lib_base, &brk_address);

    close(exec_fd);
    brk((void*)brk_address);

    if (LIKELY(syscalls_start))
    {
        extern int pinsyscalls(void* start, size_t size);
        if (UNLIKELY(pinsyscalls(syscalls_start, syscalls_size)))
        {
            die("pinsyscalls(%p, %#lu): %s", syscalls_start, syscalls_size, strerror(errno));
        }
    }

    // Load original stack ptr so the executable will
    // see argc, argv, envp as was setup by kernel
    // and jump to the entry
    asm volatile(
        "mov $0, %%ebp;"
        "mov %1, %%esp;"
        "jmp *%0"
        :: "r" (AUX_GET(AT_ENTRY)), "r" (stack_ptr)
        : "memory");

    __builtin_trap();
}

__attribute__((noreturn)) int main(int argc, char* argv[], char* envp[])
{
    loader_main(
        argc,
        argv,
        envp,
        SHIFT_AS(elf32_auxv_t**, &envp, 4),
        &argc);
}

__attribute__((naked,noreturn,used)) int _start()
{
    asm volatile("call main@plt;");
}
