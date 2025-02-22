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
        case ET_DYN: return "dynamic";
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

    if (unlikely(elf_data->header.e_machine != EM_386))
    {
        log_debug(DEBUG_ELF, "%s: not a proper machine: %#x", name, elf_data->header.e_machine);
        return -ENOEXEC;
    }

    if (unlikely(!(page = page_alloc(1, 0))))
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

struct elf_params
{
    elf32_phdr_t* phdr;
    size_t phnum;
    uintptr_t base;
    uintptr_t phdr_vaddr;
};

typedef struct elf_params elf_params_t;

static int elf_program_headers_load(file_t* file, binary_t* bin, elf_params_t* params)
{
    int errno;
    elf32_phdr_t* phdr = params->phdr;
    uintptr_t base = params->base;

    for (uint32_t i = 0; i < params->phnum; ++i)
    {
        log_debug(DEBUG_ELF, "%8s %#010x %#010x %#010x %#010x %#010x %#x %#x", elf_phdr_type_get(phdr[i].p_type),
            phdr[i].p_offset,
            phdr[i].p_vaddr,
            phdr[i].p_paddr,
            phdr[i].p_filesz,
            phdr[i].p_memsz,
            phdr[i].p_flags,
            phdr[i].p_align);

        switch (phdr[i].p_type)
        {
            case PT_LOAD:
                break;
            case PT_PHDR:
                params->phdr_vaddr = phdr[i].p_vaddr;
                fallthrough;
            default:
                continue;
        }

        int flags = mmap_flags_get(&phdr[i]);
        uintptr_t vaddr_start = phdr[i].p_vaddr + base;
        uintptr_t vaddr_page_start = vaddr_start & ~PAGE_MASK;
        uintptr_t vaddr_file_end = vaddr_start + phdr[i].p_filesz;
        uintptr_t vaddr_file_aligned_end = page_align(vaddr_file_end);
        uintptr_t vaddr_page_end = page_align(vaddr_start + phdr[i].p_memsz);

        void* ptr = do_mmap(
            ptr(vaddr_page_start),
            vaddr_file_end - vaddr_page_start,
            flags,
            MAP_PRIVATE | MAP_FIXED,
            file,
            phdr[i].p_offset & ~PAGE_MASK);

        if (unlikely(errno = errno_get(ptr)))
        {
            log_debug(DEBUG_ELF, "failed mmap: %d", errno);
            return errno;
        }

        if (vaddr_file_aligned_end != vaddr_page_end)
        {
            void* ptr = do_mmap(
                ptr(vaddr_file_aligned_end),
                vaddr_page_end - vaddr_file_aligned_end,
                flags,
                MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS,
                NULL,
                0);

            if (unlikely(errno = errno_get(ptr)))
            {
                log_debug(DEBUG_ELF, "failed mmap: %d", errno);
                return errno;
            }
        }

        bin->brk = page_align(phdr[i].p_vaddr + phdr[i].p_memsz + base);

        if (phdr[i].p_flags & PF_X && !bin->code_start)
        {
            bin->code_start = vaddr_page_start;
            bin->code_end = vaddr_page_end;
        }
    }

    return 0;
}

static int elf_load(file_t* file, binary_t* bin, void* data, argvecs_t argvecs)
{
    int errno;
    elf_data_t* elf_data = data;
    elf32_header_t* header = &elf_data->header;
    elf32_phdr_t* phdr = page_virt_ptr(elf_data->header_page);
    uintptr_t base = 0;

    if (header->e_type == ET_DYN)
    {
        // TODO: implement ASLR
        base = 0x1000;
    }

    elf_params_t params = {
        .base = base,
        .phdr = phdr,
        .phnum = header->e_phnum,
    };

    if (unlikely(errno = elf_program_headers_load(file, bin, &params)))
    {
        return errno;
    }

    bin->entry = ptr(header->e_entry + base);

    log_debug(DEBUG_ELF, "entry: %p", bin->entry);

    if (!aux_insert(AT_ENTRY, addr(bin->entry), argvecs) ||
        !aux_insert(AT_PHDR, base + params.phdr_vaddr, argvecs) ||
        !aux_insert(AT_PHENT, header->e_phentsize, argvecs) ||
        !aux_insert(AT_PHNUM, header->e_phnum, argvecs))
    {
        return -ENOMEM;
    }

    return errno;
}

static int elf_interp_load(file_t* file, binary_t* bin, void* data, argvecs_t argvecs)
{
    int errno;
    elf_data_t* elf_data = data;
    elf32_header_t* header = &elf_data->header;
    elf32_phdr_t* phdr = page_virt_ptr(elf_data->header_page);

    uintptr_t base = CONFIG_SEGMEXEC
        ? 0x5ff80000
        : 0xbff00000;

    elf_params_t params = {
        .base = base,
        .phdr = phdr,
        .phnum = header->e_phnum,
    };

    if (unlikely(errno = elf_program_headers_load(file, bin, &params)))
    {
        return errno;
    }

    bin->entry = ptr(header->e_entry + base);

    if (unlikely(!aux_insert(AT_BASE, base, argvecs)))
    {
        return -ENOMEM;
    }

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

typedef void (*ctor_t)();
typedef struct elf_module_data elf_module_data_t;

struct elf_module_data
{
    page_t* module_pages;
    uint32_t* got;
};

static NOINLINE void module_breakpoint(const char* name, uintptr_t base_address)
{
    asm volatile("" :: "a" (name), "c" (base_address) : "memory");
}

int elf_module_load(const char* name, file_t* file, kmod_t** module)
{
    int errno;
    elf32_header_t* header;
    const char* symstr = NULL;
    const char* shstr = NULL;
    elf32_sym_t* symbols = NULL;
    uint32_t* got = NULL;
    size_t size = file->dentry->inode->size;
    page_t* module_pages = page_alloc(page_align(size) / PAGE_SIZE, PAGE_ALLOC_CONT);
    ctor_t* ctors = NULL;
    size_t ctors_count = 0;
    ctor_t* dtors = NULL;
    size_t dtors_count = 0;

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

        if (shdr->sh_type == SHT_PROGBITS)
        {
            if (!strcmp(".modules_data", shstr + shdr->sh_name))
            {
                *module = shift_as(kmod_t*, header, shdr->sh_offset);
            }
            else if (!strcmp(".ctors", shstr + shdr->sh_name))
            {
                ctors = shift_as(ctor_t*, header, shdr->sh_offset);
                ctors_count = shdr->sh_size / sizeof(*ctors);
            }
            else if (!strcmp(".dtors", shstr + shdr->sh_name))
            {
                dtors = shift_as(ctor_t*, header, shdr->sh_offset);
                dtors_count = shdr->sh_size / sizeof(*dtors);
            }
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

            // FIXME: there's a bug related to .bss - it has 0 size in the ELF itself, but it
            // requires space in actual memory, so currently it's memory simply overlaps with
            // the next section
            uintptr_t relocated;
            int type = ELF32_R_TYPE(rel->r_info);
            const char* type_str = NULL;
            uintptr_t P = real_addr + rel->r_offset;
            uintptr_t A = *(uint32_t*)P;
            uintptr_t GOT = addr(got);
            uintptr_t S = shift_as(uint32_t, header, symbol_section->sh_offset) + s->st_value;
            uintptr_t G = addr(got_ptr) - addr(got);
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
                        log_warning("%s: undefined reference to %s", name, symstr + s->st_name);
                        errno = -ENOEXEC;
                        continue;
                    }

                    relocated = kernel_symbol->address + A - P;
                    break;
                case R_386_GOT32X: // G + A
                case R_386_GOT32:
                    type_str = "R_386_GOT32";
                    kernel_symbol = ksym_find_by_name(symstr + s->st_name);

                    if (unlikely(!kernel_symbol))
                    {
                        log_warning("%s: undefined reference to %s", name, symstr + s->st_name);
                        errno = -ENOEXEC;
                        continue;
                    }

                    *got_ptr++ = kernel_symbol->address;
                    relocated = G + A;
                    break;
                default:
                    log_debug(DEBUG_ELF, "  unsupported relocation type: %u; symbol %s", type, symstr + s->st_name);
                    continue;
            }

            log_debug(DEBUG_ELF, "  %-16s %02x, off: %04x: %s: at %#x (from section %s at offset %#x): setting to %#x",
                type_str,
                type,
                rel->r_offset,
                s->st_name ? symstr + s->st_name : "<unnamed>",
                P,
                shstr + symbol_section->sh_name,
                s->st_value,
                relocated);

            *(uint32_t*)P = relocated;
        }
    }

    if (unlikely(errno))
    {
        goto error;
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
    (*module)->ctors = ctors;
    (*module)->ctors_count = ctors_count;
    (*module)->dtors = dtors;
    (*module)->dtors_count = dtors_count;

    module_breakpoint(name, page_virt(module_pages));

    return 0;

error:
    if (got)
    {
        delete_array(got, GOT_ENTRIES);
    }
    pages_free(module_pages);
    return errno;
}

UNMAP_AFTER_INIT void elf_register(void)
{
    int errno;
    if (unlikely(errno = binfmt_register(&elf)))
    {
        log_warning("failed to register elf binfmt: %d", errno);
    }
}
