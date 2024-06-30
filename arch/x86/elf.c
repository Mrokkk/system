#include <kernel/vm.h>
#include <kernel/elf.h>
#include <kernel/exec.h>
#include <kernel/ksyms.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/api/elf.h>
#include <kernel/process.h>
#include <kernel/backtrace.h>

#include <arch/mmap.h>

#define DEBUG_ELF 0

static int elf_prepare(const char* name, file_t* file, string_t** interpreter, void** data);
static int elf_load(file_t* file, binary_t* bin, void* data, argvecs_t argvecs);
static int elf_interp_load(file_t* file, binary_t* bin, void* data, argvecs_t argvecs);
static void elf_cleanup(void* data);

static binfmt_t elf = {
    .name = "elf",
    .signature = ELFMAG0,
    .prepare = &elf_prepare,
    .load = &elf_load,
    .interp_load = &elf_interp_load,
    .cleanup = &elf_cleanup,
};

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
    bool is_valid =
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

static char* elf_phdr_type_get(uint32_t type)
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

struct elf_data
{
    elf32_header_t header;
    page_t* header_page;
};

typedef struct elf_data elf_data_t;

static int elf_prepare(const char* name, file_t* file, string_t** interpreter, void** data)
{
    int errno;
    page_t* page;
    elf32_phdr_t* phdr;
    elf_data_t* elf_data = fmalloc(sizeof(*elf_data));

    if (unlikely(!elf_data))
    {
        return -ENOMEM;
    }

    if ((errno = do_read(file, 0, &elf_data->header, sizeof(elf32_header_t))))
    {
        log_debug(DEBUG_ELF, "%s: cannot read header: %d", name, errno);
        goto free_elf_data;
    }

    if (elf_validate(name, &elf_data->header))
    {
        log_debug(DEBUG_ELF, "%s: not an ELF", name);
        return -ENOEXEC;
    }

    // FIXME: instead of allocating page for phdhr, reading it from fs, and
    // then mmaping it for a user, I could do mmap here. But I need to cleanup
    // the mess with exec_prepare_initial_vma
    if (unlikely(!(page = page_alloc1())))
    {
        log_debug(DEBUG_ELF, "%s: cannot allocate page for phdr", name);
        errno = -ENOMEM;
        goto free_elf_data;
    }

    phdr = page_virt_ptr(page);

    if ((errno = do_read(file, elf_data->header.e_phoff, phdr, elf_data->header.e_phentsize * elf_data->header.e_phnum)))
    {
        log_debug(DEBUG_ELF, "%s: cannot read phdr: %d", name, errno);
        goto free_phdr;
    }

    for (size_t i = 0; i < elf_data->header.e_phnum; ++i)
    {
        if (phdr[i].p_type == PT_INTERP)
        {
            if (unlikely(errno = string_read(file, phdr[i].p_offset, phdr[i].p_memsz, interpreter)))
            {
                log_debug(DEBUG_ELF, "%s: cannot read interpreter path", name);
                goto free_phdr;
            }
        }
    }

    elf_data->header_page = page;

    *data = elf_data;

    return 0;

free_phdr:
    pages_free(page);
free_elf_data:
    return errno;
}

static void elf_cleanup(void* data)
{
    elf_data_t* elf_data = data;

    pages_free(elf_data->header_page);
    ffree(elf_data, sizeof(*elf_data));
}

int elf_header_read(const char* name, file_t* file, elf32_header_t* header)
{
    int errno;

    if ((errno = do_read(file, 0, header, sizeof(elf32_header_t))))
    {
        log_debug(DEBUG_ELF, "%s: cannot read header: %d", name, errno);
        return errno;
    }

    if (elf_validate(name, header))
    {
        log_debug(DEBUG_ELF, "%s: not an ELF", name);
        return -ENOEXEC;
    }

    return 0;
}

static int elf_program_headers_load(file_t* file, elf32_phdr_t* phdr, size_t phnum, binary_t* bin, uintptr_t base)
{
    int errno;

    for (uint32_t i = 0; i < phnum; ++i)
    {
        log_debug(DEBUG_ELF, "%8s %08x %08x %08x %08x %08x %x %x", elf_phdr_type_get(phdr[i].p_type),
            phdr[i].p_offset,
            phdr[i].p_vaddr,
            phdr[i].p_paddr,
            phdr[i].p_filesz,
            phdr[i].p_memsz,
            phdr[i].p_flags,
            phdr[i].p_align);

        if (phdr[i].p_type != PT_LOAD)
        {
            continue;
        }

        uint32_t vaddr_start = phdr[i].p_vaddr + base;
        uint32_t vaddr_file_end = vaddr_start + phdr[i].p_filesz;
        uint32_t vaddr_page_start = vaddr_start & ~PAGE_MASK;
        uint32_t vaddr_page_end = page_align(vaddr_start + phdr[i].p_memsz);

        void* ptr = do_mmap(
            ptr(vaddr_page_start),
            page_align(phdr[i].p_memsz),
            mmap_flags_get(&phdr[i]),
            MAP_PRIVATE | MAP_FIXED,
            file,
            phdr[i].p_offset & ~PAGE_MASK);

        if ((errno = errno_get(ptr)))
        {
            log_debug(DEBUG_ELF, "failed mmap: %d", ptr);
            return errno;
        }

        if (phdr[i].p_flags & PF_W)
        {
            memset(ptr(vaddr_file_end), 0, vaddr_page_end - vaddr_file_end);
        }

        bin->brk = page_align(phdr[i].p_vaddr + phdr[i].p_memsz + base);
    }

    return 0;
}

static int elf_load(file_t* file, binary_t* bin, void* data, argvecs_t argvecs)
{
    int errno;
    elf_data_t* elf_data = data;
    elf32_header_t* header = &elf_data->header;
    elf32_phdr_t* phdr = page_virt_ptr(elf_data->header_page);
    uint32_t base = 0;

    // FIXME: this breaks running process if there's some failure
    // in do_mmap below. I should save previous vmas and restore
    // then in case of failure
    bin->stack_vma = exec_prepare_initial_vma();

    if (header->e_type == ET_DYN)
    {
        // TODO: implement ASLR
        base = 0x1000;
    }

    if ((errno = elf_program_headers_load(file, phdr, header->e_phnum, bin, base)))
    {
        return errno;
    }

    bin->entry = ptr(header->e_entry + base);

    log_debug(DEBUG_ELF, "entry: %x", bin->entry);

    bin->inode = file->inode;

    void* phdr_mapped = do_mmap(
        NULL,
        header->e_phentsize * header->e_phnum,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        NULL,
        0);

    if (unlikely(errno = errno_get(phdr_mapped)))
    {
        log_debug(DEBUG_ELF, "failed to map PHDR: %d", errno);
        return errno;
    }

    memcpy(phdr_mapped, phdr, header->e_phentsize * header->e_phnum);

    if (aux_insert(AT_ENTRY, addr(bin->entry), argvecs) ||
        aux_insert(AT_PHDR, addr(phdr_mapped), argvecs) ||
        aux_insert(AT_PHENT, header->e_phentsize, argvecs) ||
        aux_insert(AT_BASE, base, argvecs) ||
        aux_insert(AT_PHNUM, header->e_phnum, argvecs))
    {
        return -ENOMEM;
    }

    return errno;
}

static int elf_interp_load(file_t* file, binary_t* bin, void* data, argvecs_t)
{
    int errno;
    elf_data_t* elf_data = data;
    elf32_header_t* header = &elf_data->header;
    elf32_phdr_t* phdr = page_virt_ptr(elf_data->header_page);

    if ((errno = elf_program_headers_load(file, phdr, header->e_phnum, bin, 0)))
    {
        return errno;
    }

    bin->entry = ptr(header->e_entry);

    return 0;
}

int elf_locate_common_sections(
    elf32_header_t* header,
    const char** symstr,
    const char** shstr,
    elf32_sym_t** symbols)
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
    elf32_sym_t* symbols = NULL;
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
            elf32_sym_t* s = &symbols[ELF32_R_SYM(rel->r_info)];
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

void elf_register(void)
{
    int errno;
    if (unlikely(errno = binfmt_register(&elf)))
    {
        log_warning("failed to register elf binfmt: %d", errno);
    }
}
