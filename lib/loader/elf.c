#include "elf.h"

#include <sys/mman.h>
#include <kernel/api/elf.h>

#include "loader.h"

static int mmap_flags_get(elf32_phdr_t* p)
{
    int mmap_flags = PROT_READ;
    if (p->p_flags & PF_W) mmap_flags |= PROT_WRITE;
    if (p->p_flags & PF_X) mmap_flags |= PROT_EXEC;
    return mmap_flags;
}

void* mmap_phdr(int exec_fd, size_t page_mask, elf32_phdr_t* phdr)
{
    void* addr = SYSCALL(mmap(
        PTR(phdr->p_vaddr & ~page_mask),
        phdr->p_memsz,
        mmap_flags_get(phdr),
        MAP_PRIVATE | (phdr->p_vaddr ? MAP_FIXED : 0),
        exec_fd,
        phdr->p_offset & ~page_mask));

    if (UNLIKELY((int)addr == -1))
    {
        die_errno("mmap");
    }

    return addr;
}

void mprotect_phdr(uintptr_t base_address, size_t page_size, int additional, elf32_phdr_t* phdr)
{
    SYSCALL(mprotect(
        PTR(phdr->p_vaddr + base_address),
        ALIGN_TO(phdr->p_memsz, page_size),
        mmap_flags_get(phdr) | additional));
}

const char* strtab_read(strtab_t* this, uintptr_t addr)
{
    return this->strings + addr;
}
