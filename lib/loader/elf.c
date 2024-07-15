#include "elf.h"

#include <string.h>
#include <sys/mman.h>
#include <kernel/api/elf.h>

#include "loader.h"

static int mmap_flags_get(int p_flags)
{
    int mmap_flags = PROT_READ;
    if (p_flags & PF_W) mmap_flags |= PROT_WRITE;
    if (p_flags & PF_X) mmap_flags |= PROT_EXEC;
    return mmap_flags;
}

static void* mmap_wrapper(void* addr, size_t len, int prot, int flags, int fd, size_t off)
{
    void* mapped = mmap(addr, len, prot, flags, fd, off);

    if (UNLIKELY((int)mapped == -1))
    {
        char buffer[128];
        sprintf(buffer, "mmap(%p, %#x, %#x, %#x, %u, %#x)", addr, len, prot, flags, fd, off);
        die_errno(buffer);
    }

    return mapped;
}

void* mmap_phdr(int exec_fd, size_t page_size, elf32_phdr_t* phdr, int add_prot, uintptr_t base)
{
    uintptr_t page_mask = page_size - 1;
    uintptr_t vaddr_start = base + phdr->p_vaddr;
    uintptr_t vaddr_page_start = vaddr_start & ~page_mask;
    uintptr_t vaddr_file_end = vaddr_start + phdr->p_filesz;
    uintptr_t vaddr_page_end = ALIGN_TO(vaddr_start + phdr->p_memsz, page_size);

    void* mapped = mmap_wrapper(
        PTR(vaddr_page_start),
        vaddr_page_end - vaddr_page_start,
        mmap_flags_get(phdr->p_flags | add_prot),
        MAP_PRIVATE | MAP_FIXED,
        exec_fd,
        phdr->p_offset & ~page_mask);

    if (phdr->p_flags & PF_W)
    {
        memset(PTR(vaddr_file_end), 0, vaddr_page_end - vaddr_file_end);
    }

    return mapped;
}

void mprotect_phdr(uintptr_t base_address, size_t page_size, int additional, elf32_phdr_t* phdr)
{
    SYSCALL(mprotect(
        PTR(phdr->p_vaddr + base_address),
        ALIGN_TO(phdr->p_memsz, page_size),
        mmap_flags_get(phdr->p_flags | additional)));
}

const char* strtab_read(strtab_t* this, uintptr_t addr)
{
    return this->strings + addr;
}
