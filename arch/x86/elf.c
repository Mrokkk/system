#include <kernel/elf.h>
#include <kernel/kernel.h>

#include <arch/vm.h>

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

static inline void elf_shdr_flags_get(elf32_shdr_t* section, char* buffer)
{
    size_t i = 0;
#define ELF_FLAG_STR(flag, str) \
    if (section->sh_flags & flag) i += sprintf(buffer + i, str)
    i = sprintf(buffer, "{");
    ELF_FLAG_STR(SHF_WRITE, "write ");
    ELF_FLAG_STR(SHF_ALLOC, "alloc ");
    ELF_FLAG_STR(SHF_EXECINSTR, "exec ");
    ELF_FLAG_STR(SHF_MERGE, "merge ");
    ELF_FLAG_STR(SHF_STRINGS, "strings ");
    ELF_FLAG_STR(SHF_INFO_LINK, "index ");
    i += sprintf(buffer + i, "}");
}

static inline void elf_shdr_print(elf32_shdr_t* section, const char* strings)
{
    char buffer[64];
    elf_shdr_flags_get(section, buffer);
    log_debug(DEBUG_ELF, "name=%s sh_flags=%s sh_addr=%x sh_offset=%x sh_size=%x",
        &strings[section->sh_name],
        buffer,
        section->sh_addr,
        section->sh_offset,
        section->sh_size);
}

static inline int vm_flags_get(elf32_shdr_t* s)
{
    int vm_flags = VM_READ;
    if (s->sh_flags & SHF_WRITE) vm_flags |= VM_WRITE;
    if (s->sh_flags & SHF_EXECINSTR) vm_flags |= VM_EXECUTABLE;
    return vm_flags;
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

// TODO: this need a refactoring and also proper cleanup after failures

// FIXME: big issue - if process did exec a binary and this binary forks, and execs the same binary,
// page_range_get causes pages's assigned to different list, thus breaking original list

int read_elf(const char* name, void* data, vm_area_t** result_area, uint32_t* brk, void** entry)
{
    elf32_shdr_t* sections;
    elf32_shdr_t* string_table;
    elf32_header_t* header = data;
    const char* strings;

    if (elf_validate(name, header))
    {
        log_debug(DEBUG_ELF, "not an ELF: %x", data);
        return -ENOEXEC;
    }

    sections = ptr(addr(header) + header->e_shoff);
    string_table = ptr(&sections[header->e_shstrndx]);
    strings = ptr(addr(header) + string_table->sh_offset);

    vm_area_t* new_area = NULL;
    *brk = 0;

    for (int i = 0; i < header->e_shnum; ++i)
    {
        if (!sections[i].sh_addr)
        {
            continue;
        }

        elf_shdr_print(&sections[i], strings);

        // Check if address of section is within already existing vm_area
        if (new_area && sections[i].sh_addr < new_area->virt_address + new_area->size)
        {
            uint32_t new_size = page_align(sections[i].sh_addr + sections[i].sh_size - new_area->virt_address);
            uint32_t prev_size = new_area->size;

            // If exiting vm_area has lower size, extend it
            if (new_size > prev_size)
            {
                uint32_t needed_pages = (new_size - prev_size) / PAGE_SIZE;

                log_debug(DEBUG_ELF, "extending size from %x to %x", prev_size, new_size);

                page_t* page_range;

                if (sections[i].sh_flags & SHF_WRITE)
                {
                    *brk += new_size - prev_size;
                    page_range = page_alloc(needed_pages, PAGE_ALLOC_CONT);
                }
                else
                {
                    page_range = page_range_get(phys(page_align(addr(header) + sections[i].sh_addr)), needed_pages);
                }

                if (unlikely(!page_range))
                {
                    return -ENOMEM;
                }

                vm_extend(new_area, page_range, vm_flags_get(&sections[i]), needed_pages);
            }
            // Apply additional flags
            new_area->vm_flags |= vm_flags_get(&sections[i]); // FIXME: do it via vm_extend
            continue;
        }

        if (sections[i].sh_flags & SHF_WRITE)
        {
            void* dest_vaddr;
            void* src_vaddr;
            uint32_t size = page_align(sections[i].sh_size);
            page_t* first_page = page_alloc(size / PAGE_SIZE, PAGE_ALLOC_CONT);

            if (unlikely(!first_page))
            {
                log_debug(DEBUG_ELF, "cannot allocate page");
                return -ENOMEM;
            }

            src_vaddr = ptr(page_beginning(addr(header) + sections[i].sh_addr));
            dest_vaddr = page_virt_ptr(first_page);

            log_debug(DEBUG_ELF, "copying %u B from %x to %x", size, src_vaddr, dest_vaddr);
            memcpy(dest_vaddr, src_vaddr, size);

            new_area = vm_create(
                first_page,
                sections[i].sh_addr,
                size,
                vm_flags_get(&sections[i]));

            vm_add(result_area, new_area);

            *brk = sections[i].sh_addr + size;
        }
        else
        {
            uint32_t paddr = phys(page_beginning(addr(header) + sections[i].sh_addr));
            uint32_t pages = page_align(sections[i].sh_size) / PAGE_SIZE;

            new_area = vm_create(
                page_range_get(paddr, pages),
                sections[i].sh_addr,
                page_align(sections[i].sh_size),
                vm_flags_get(&sections[i]) | VM_NONFREEABLE);

            vm_add(result_area, new_area);
        }
    }

    *entry = ptr(header->e_entry);

    return 0;
}
