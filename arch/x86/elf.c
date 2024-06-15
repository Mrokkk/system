#include <kernel/vm.h>
#include <kernel/elf.h>
#include <kernel/ksyms.h>
#include <kernel/usyms.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/process.h>
#include <kernel/backtrace.h>

#include <arch/mmap.h>

static int elf_symbols_read(const elf32_header_t* header, file_t* file, page_t** pages);

static inline char* elf_get_architecture(uint8_t v)
{
    switch (v)
    {
        case ELFCLASS32: return "32-bit";
        case ELFCLASS64: return "64-bit";
        default: return "Unknown architecture";
    }
}

static inline char* elf_get_bitorder(uint8_t v)
{
    switch (v)
    {
        case ELFDATA2LSB: return "LSB";
        case ELFDATA2MSB: return "MSB";
        default: return "Unknown bit order";
    }
}

static inline char* elf_get_type(uint8_t v)
{
    switch (v)
    {
        case ET_REL: return "relocatable";
        case ET_EXEC: return "executable";
        default: return "unknown type";
    }
}

static inline char* elf_get_machine(uint8_t v)
{
    switch (v)
    {
        case EM_386: return "Intel 80386";
        default: return "unknown machine";
    }
}

static inline void elf_header_print(const char* name, const elf32_header_t* header)
{
    log_debug(DEBUG_ELF, "%s: ELF %s %s %s, %s",
        name,
        elf_get_architecture(header->e_ident[EI_CLASS]),
        elf_get_bitorder(header->e_ident[EI_DATA]),
        elf_get_type(header->e_type),
        elf_get_machine(header->e_machine));
}

static inline int elf_validate(const char* name, const elf32_header_t* header)
{
    int is_valid =
        header->e_ident[EI_MAG0] == ELFMAG0 &&
        header->e_ident[EI_MAG1] == ELFMAG1 &&
        header->e_ident[EI_MAG2] == ELFMAG2 &&
        header->e_ident[EI_MAG3] == ELFMAG3;

    if (!is_valid)
    {
        return 1;
    }

    elf_header_print(name, header);
    return 0;
}

char* elf_phdr_type_get(uint32_t type)
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

static inline int mmap_flags_get(elf32_phdr_t* p)
{
    int mmap_flags = PROT_READ;
    if (p->p_flags & PF_W) mmap_flags |= PROT_WRITE;
    if (p->p_flags & PF_X) mmap_flags |= PROT_EXEC;
    return mmap_flags;
}

int elf_load(const char* name, file_t* file, binary_t* bin)
{
    int errno;
    page_t* page;
    elf32_phdr_t* phdr;
    elf32_header_t header;

    if ((errno = do_read(file, 0, &header, sizeof(elf32_header_t))))
    {
        log_debug(DEBUG_ELF, "%s: cannot read header: %d", name, errno);
        return errno;
    }

    if (elf_validate(name, &header))
    {
        log_debug(DEBUG_ELF, "%s: not an ELF", name);
        return -ENOEXEC;
    }

    if (unlikely(!(page = page_alloc1())))
    {
        log_debug(DEBUG_ELF, "%s: cannot allocate page for phdr", name);
        return -ENOMEM;
    }

    phdr = page_virt_ptr(page);

    if ((errno = do_read(file, header.e_phoff, phdr, header.e_phentsize * header.e_phnum)))
    {
        log_debug(DEBUG_ELF, "%s: cannot read phdr: %d", name, errno);
        return errno;
    }

    bin->stack_vma = exec_prepare_initial_vma();

    for (uint32_t i = 0; i < header.e_phnum; ++i)
    {
        log_debug(DEBUG_ELF, "%8s %08x %08x %08x %08x %08x %x %x", elf_phdr_type_get(phdr[i].p_type),
            phdr[i].p_offset,
            phdr[i].p_vaddr,
            phdr[i].p_paddr,
            phdr[i].p_filesz,
            phdr[i].p_memsz,
            phdr[i].p_flags,
            phdr[i].p_align);

        if (phdr[i].p_type == PT_DYNAMIC)
        {
        }

        if (phdr[i].p_type != PT_LOAD)
        {
            continue;
        }

        void* ptr = do_mmap(
            ptr(phdr[i].p_vaddr),
            phdr[i].p_memsz,
            mmap_flags_get(phdr + i),
            MAP_PRIVATE | MAP_FIXED,
            file,
            phdr[i].p_offset);

        if ((errno = errno_get(ptr)))
        {
            log_debug(DEBUG_ELF, "failed mmap: %d", ptr);
            page_free(phdr);
            return errno;
        }

        bin->brk = page_align(phdr[i].p_vaddr + phdr[i].p_memsz);
    }

    bin->entry = ptr(header.e_entry);
    pages_free(page);

    log_debug(DEBUG_ELF, "entry: %x", bin->entry);

    if (DEBUG_BTUSER)
    {
        elf_symbols_read(&header, file, &bin->symbols_pages);
    }
    else
    {
        bin->symbols_pages = NULL;
    }

    return 0;
}

static int elf_section_read(const elf32_shdr_t* shdr, file_t* file, page_t** pages)
{
    int errno;
    size_t page_count = page_align(shdr->sh_size) / PAGE_SIZE;

    *pages = page_alloc(page_count, page_count == 1 ? PAGE_ALLOC_DISCONT : PAGE_ALLOC_CONT);

    if (unlikely(!*pages))
    {
        return -ENOMEM;
    }

    if (unlikely(errno = do_read(file, shdr->sh_offset, page_virt_ptr(*pages), shdr->sh_size)))
    {
        return errno;
    }

    return 0;
}

static void elf_symbols_fill(page_t* pages, const elf32_sym* sym, const size_t sym_count, const char* strings)
{
    size_t i;
    usym_t* usym = page_virt_ptr(pages);

    for (i = 0; i < sym_count; ++i)
    {
        const char* name = strings + sym[i].st_name;
        size_t len = strlen(name);

        usym->name = ptr(addr(usym) + sizeof(usym_t));
        usym->start = sym[i].st_value;
        usym->end = usym->start + sym[i].st_size;
        usym->next = i == sym_count - 1
            ? 0
            : ptr(align(addr(usym) + sizeof(usym_t) + len + 1, 4));

        strcpy(usym->name, name);

        usym = usym->next;
    }

    log_debug(DEBUG_ELF, "read %u symbols", i);
}

static int elf_symbols_read(const elf32_header_t* header, file_t* file, page_t** pages)
{
    int errno = -ENOMEM;
    page_t* sht_pages = NULL;
    page_t* syms_pages = NULL;
    page_t* str_pages = NULL;
    page_t* usyms_pages;
    elf32_sym* sym = NULL;
    char* strings = NULL;
    size_t sym_count = 0, strings_len = 0;

    sht_pages = page_alloc(page_align(header->e_shentsize * header->e_shnum) / PAGE_SIZE, PAGE_ALLOC_CONT);

    if (unlikely(!sht_pages))
    {
        return errno;
    }

    elf32_shdr_t* shdrt = page_virt_ptr(sht_pages);

    if (unlikely(errno = do_read(file, header->e_shoff, shdrt, header->e_shentsize * header->e_shnum)))
    {
        goto free_sht;
    }

    for (size_t i = 0; i < header->e_shnum; ++i)
    {
        elf32_shdr_t* shdr = &shdrt[i];

        switch (shdr->sh_type)
        {
            case SHT_SYMTAB:
                if (unlikely(errno = elf_section_read(shdr, file, &syms_pages)))
                {
                    goto free;
                }
                sym = page_virt_ptr(syms_pages);
                sym_count = shdr->sh_size / sizeof(elf32_sym);
                break;
            case SHT_STRTAB:
                if (strings) continue;
                if (unlikely(errno = elf_section_read(shdr, file, &str_pages)))
                {
                    goto free;
                }
                strings = page_virt_ptr(str_pages);
                strings_len = shdr->sh_size;
                break;
        }
    }

    if (unlikely(!sym || !strings))
    {
        errno = -ENOEXEC;
        goto free;
    }

    // sym_count * 4 is the maximum padding + '\0' which can be added to the usym_t
    usyms_pages = page_alloc(
        page_align(sizeof(usym_t) * sym_count + strings_len + sym_count * 4) / PAGE_SIZE,
        PAGE_ALLOC_CONT);

    if (unlikely(!usyms_pages))
    {
        errno = -ENOEXEC;
        goto free;
    }

    *pages = usyms_pages;

    elf_symbols_fill(usyms_pages, sym, sym_count, strings);

    errno = 0;

free:
    if (str_pages)
    {
        ASSERT(pages_free(str_pages));
    }
    if (syms_pages)
    {
        ASSERT(pages_free(syms_pages));
    }
free_sht:
    ASSERT(pages_free(sht_pages));
    return errno;
}

int elf_locate_common_sections(
    elf32_header_t* header,
    const char** symstr,
    const char** shstr,
    elf32_sym** symbols)
{
    elf32_shdr_t* shdrt = shift_as(elf32_shdr_t*, header, header->e_shoff);
    int count = 0;
    for (size_t i = 0; i < header->e_shnum; ++i)
    {
        elf32_shdr_t* shdr = &shdrt[i];
        void* section_data = shift_as(char*, header, shdr->sh_offset);

        switch (shdr->sh_type)
        {
            case SHT_STRTAB:
                if (i == header->e_shstrndx)
                {
                    *shstr = section_data;
                }
                else
                {
                    *symstr = section_data;
                }
                ++count;
                break;
            case SHT_SYMTAB:
                *symbols = section_data;
                ++count;
                break;
        }
    }

    return count != 3;
}

#define GOT_ENTRIES 32

typedef struct elf_module_data elf_module_data_t;

struct elf_module_data
{
    page_t* module_pages;
    uint32_t* got;
};

int elf_module_load(const char* name, file_t* file, kmod_t** module)
{
    int errno;
    elf32_header_t* header;
    const char* symstr = NULL;
    const char* shstr = NULL;
    elf32_sym* symbols = NULL;
    uint32_t* got = NULL;
    size_t size = file->inode->size;
    page_t* module_pages = page_alloc(page_align(size) / PAGE_SIZE, PAGE_ALLOC_CONT);

    if (unlikely(!module_pages))
    {
        log_debug(DEBUG_ELF, "%s: no memory to load module", name);
        return -ENOMEM;
    }

    if ((errno = do_read(file, 0, page_virt_ptr(module_pages), size)))
    {
        log_debug(DEBUG_ELF, "%s: cannot read module: %d", name, errno);
        goto error;
    }

    *module = NULL;
    header = page_virt_ptr(module_pages);

    if (elf_validate(name, header))
    {
        log_debug(DEBUG_ELF, "%s: not an ELF", name);
        errno = -ENOEXEC;
        goto error;
    }

    elf32_shdr_t* shdrt = shift_as(elf32_shdr_t*, header, header->e_shoff);

    if (elf_locate_common_sections(header, &symstr, &shstr, &symbols))
    {
        log_debug(DEBUG_ELF, "%s: missing sections", name);
        errno = -ENOEXEC;
        goto error;
    }

    got = alloc_array(uint32_t, GOT_ENTRIES);
    uint32_t* got_ptr = got;

    for (size_t i = 0; i < header->e_shnum; ++i)
    {
        elf32_shdr_t* shdr = &shdrt[i];

        if (shdr->sh_type == SHT_PROGBITS && !strcmp(".modules_data", shstr + shdr->sh_name))
        {
            *module = shift_as(kmod_t*, header, shdr->sh_offset);
            continue;
        }
        else if (shdr->sh_type != SHT_REL)
        {
            continue;
        }

        elf32_shdr_t* real = &shdrt[shdr->sh_info];
        uint32_t real_addr = shift_as(uint32_t, header, real->sh_offset);
        elf32_shdr_t* symbol_section;

        log_debug(DEBUG_ELF, "REL: sh_name=%s for %s",
            shstr + shdr->sh_name,
            shstr + real->sh_name);

        elf32_rel_t* rel = shift_as(elf32_rel_t*, header, shdr->sh_offset);

        for (size_t i = 0; i < shdr->sh_size / shdr->sh_entsize; ++i, ++rel)
        {
            elf32_sym* s = &symbols[ELF32_R_SYM(rel->r_info)];
            symbol_section = &shdrt[s->st_shndx];

            uint32_t relocated;
            int type = ELF32_R_TYPE(rel->r_info);
            const char* type_str = NULL;
            uint32_t P = real_addr + rel->r_offset;
            uint32_t A = *(uint32_t*)P + s->st_value; // FIXME: should it be like that?
            uint32_t GOT = addr(got);
            uint32_t S = shift_as(uint32_t, header, symbol_section->sh_offset);
            uint32_t G = addr(got_ptr) - addr(got);
            ksym_t* kernel_symbol;

            if (unlikely(G >= GOT_ENTRIES * sizeof(uint32_t)))
            {
                log_warning("%s: no more entries in GOT", name);
                break;
            }

            switch (type)
            {
                case R_386_32: // S + A
                    type_str = "R_386_32";
                    relocated = S + A;
                    break;
                case R_386_PC32: // S + A - P
                    type_str = "R_386_PC32";
                    relocated = S + A - P;
                    break;
                case R_386_GOTPC: // GOT + A - P
                    type_str = "R_386_GOTPC";
                    relocated = GOT + A - P;
                    break;
                case R_386_GOTOFF: // S + A - GOT
                    type_str = "R_386_GOTOFF";
                    relocated = S + A - GOT;
                    break;
                case R_386_PLT32: // L + A - P
                    type_str = "R_386_PLT32";
                    kernel_symbol = ksym_find_by_name(symstr + s->st_name);

                    if (unlikely(!kernel_symbol))
                    {
                        log_info("%s: undefined reference to %s", name, symstr + s->st_name);
                        errno = -ENOEXEC;
                        goto error;
                    }

                    relocated = kernel_symbol->address + A - P;
                    break;
                case R_386_GOT32X: // G + A
                case R_386_GOT32:
                    type_str = "R_386_GOT32";
                    kernel_symbol = ksym_find_by_name(symstr + s->st_name);

                    if (unlikely(!kernel_symbol))
                    {
                        log_info("%s: undefined reference to %s", name, symstr + s->st_name);
                        errno = -ENOEXEC;
                        goto error;
                    }

                    *got_ptr++ = kernel_symbol->address;
                    relocated = G + A;
                    break;
                default:
                    log_debug(DEBUG_ELF, "  unsupported relocation type: %u; symbol %s", type, symstr + s->st_name);
                    continue;
            }

            log_debug(DEBUG_ELF, "  %-16s %02x, off: %04x: %s: relocated %x", type_str, type, rel->r_offset,
                s->st_name ? symstr + s->st_name : shstr + symbol_section->sh_name,
                relocated);

            *(uint32_t*)(real_addr + rel->r_offset) = relocated;
        }
    }

    if (unlikely(!*module))
    {
        log_warning("%s: not a module", name);
        errno = -ENOEXEC;
        goto error;
    }

    elf_module_data_t* data = alloc(typeof(*data), ({ this->module_pages = module_pages; this->got = got; }));

    if (unlikely(!data))
    {
        log_debug(DEBUG_ELF, "cannot allocate memory for elf_module_data_t");
        errno = -ENOMEM;
        goto error;
    }

    (*module)->data = data;

    return 0;

error:
    if (got)
    {
        delete_array(got, GOT_ENTRIES);
    }
    pages_free(module_pages);
    return errno;
}
