#include "loader.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <kernel/api/elf.h>

#include "lib.h"
#include "elf.h"
#include "list.h"

int debug;
static auxv_t saved_auxv;
static LIST_DECLARE(libs);

#define STORE(what, where) \
    case what: where = value; break

#define AUX_STORE(a) \
    case a: \
        saved_auxv._##a = aux[i].a_un.a_val; break

#define AUX_VERIFY(a) \
    do { if (UNLIKELY(!saved_auxv._##a)) die("missing " #a); } while (0)

#define AUX_GET(a) \
    ({ saved_auxv._##a; })

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
            AUX_STORE(AT_BASE);
        }
    }

    AUX_VERIFY(AT_EXECFD);
    AUX_VERIFY(AT_PHDR);
    AUX_VERIFY(AT_PAGESZ);
    AUX_VERIFY(AT_PHNUM);
    AUX_VERIFY(AT_ENTRY);
    AUX_VERIFY(AT_BASE);
}

static void link(dynamic_t* dynamic, int exec_fd, uintptr_t base_address)
{
    STRTAB_DECLARE(dynstr);
    REL_DECLARE(rel);

    for (size_t i = 0; i < dynamic->count && dynamic->entries[i].d_tag; ++i)
    {
        elf32_dyn_t* entry = &dynamic->entries[i];
        uint32_t value = entry->d_un.d_val;
        switch (entry->d_tag)
        {
            case DT_NEEDED:
            {
                lib_t* lib = ALLOC(malloc(sizeof(*lib)));
                list_init(&lib->list_entry);
                lib->string_ndx = value;
                list_add_tail(&lib->list_entry, &libs);
                break;
            }
            STORE(DT_REL, rel.offset);
            STORE(DT_RELSZ, rel.size);
            STORE(DT_RELENT, rel.entsize);
            STORE(DT_STRSZ, dynstr.size);
            STORE(DT_STRTAB, dynstr.offset);
        }
    }

    dynstr.strings = alloc_read(exec_fd, dynstr.size, dynstr.offset);
    rel.entries = alloc_read(exec_fd, rel.size, rel.offset);

    for (size_t i = 0; i < rel.size / rel.entsize; ++i)
    {
        int type = ELF32_R_TYPE(rel.entries[i].r_info);
        if (type != R_386_RELATIVE)
        {
            printf("warning: unsupported rel type: %#x\n", type);
            continue;
        }

        *(uintptr_t*)(rel.entries[i].r_offset + base_address) += base_address;
    }

    lib_t* lib;
    list_for_each_entry(lib, &libs, list_entry)
    {
        int lib_fd;
        char path[128];
        elf32_phdr_t* phdr;
        elf32_header_t* header;

        sprintf(path, "/bin/%s", dynstr.read(&dynstr, lib->string_ndx));

        lib_fd = SYSCALL(open(path, O_RDONLY));

        header = alloc_read(lib_fd, sizeof(*header), 0);
        phdr = alloc_read(lib_fd, header->e_phentsize * header->e_phnum, header->e_phoff);

        phdr_print(phdr, header->e_phnum);

        close(lib_fd);
    }
}

static entry_t elf_load(elf32_auxv_t** auxv)
{
    int exec_fd = 0;
    elf32_phdr_t* phdr = NULL;
    uintptr_t brk_address = 0;
    uintptr_t base_address = 0;
    DYNAMIC_DECLARE(dynamic);

    auxv_read(auxv);

    exec_fd = AUX_GET(AT_EXECFD);
    phdr = PTR(AUX_GET(AT_PHDR));
    base_address = AUX_GET(AT_BASE);

    phdr_print(phdr, AUX_GET(AT_PHNUM));

    for (size_t i = 0; i < AUX_GET(AT_PHNUM); ++i)
    {
        switch (phdr[i].p_type)
        {
            case PT_DYNAMIC:
                dynamic.entries = alloc_read(exec_fd, dynamic.size = phdr[i].p_filesz, phdr[i].p_offset);
                dynamic.count = dynamic.size / sizeof(elf32_dyn_t);
                break;

            case PT_LOAD:
                brk_address = phdr[i].p_memsz + phdr[i].p_vaddr + base_address;

                // Allow writing over segment to perform relocations
                if (phdr[i].p_flags & PF_X)
                {
                    mprotect_phdr(base_address, AUX_GET(AT_PAGESZ), PROT_WRITE, &phdr[i]);
                }
        }
    }

    link(&dynamic, exec_fd, AUX_GET(AT_BASE));

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

    return (entry_t)AUX_GET(AT_ENTRY);
}

__attribute__((noreturn)) int main(int argc, char*[], char* envp[])
{
    debug = !!getenv("L");

    entry_t entry = elf_load(SHIFT_AS(elf32_auxv_t**, &envp, 4));

    asm volatile(
        "mov %1, %%esp;"
        "jmp *%0"
        :: "r" (entry), "r" (&argc)
        : "memory");

    while (1);
}

__attribute__((naked,noreturn,used)) int _start()
{
    asm volatile(
        "call bss_init;"
        "call __libc_start_main;"
        "call main;");
}
