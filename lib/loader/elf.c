#include "elf.h"

#include <sys/mman.h>
#include <kernel/api/elf.h>

#include <syslog.h>

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

    if (UNLIKELY(mapped == MAP_FAILED))
    {
        char buffer[128];
        sprintf(buffer, "mmap(%p, %#x, %#x, %#x, %u, %#x)", addr, len, prot, flags, fd, off);
        die_errno(buffer);
    }

    return mapped;
}

void* mmap_phdr(int exec_fd, size_t page_size, elf32_phdr_t* phdr, uintptr_t base)
{
    uintptr_t page_mask = page_size - 1;
    uintptr_t vaddr_start = base + phdr->p_vaddr;
    uintptr_t vaddr_page_start = vaddr_start & ~page_mask;
    uintptr_t vaddr_file_end = vaddr_start + phdr->p_filesz;
    uintptr_t vaddr_file_aligned_end = ALIGN_TO(vaddr_file_end, page_size);
    uintptr_t vaddr_page_end = ALIGN_TO(vaddr_start + phdr->p_memsz, page_size);

    int flags = mmap_flags_get(phdr->p_flags);

    mmap_wrapper(
        PTR(vaddr_page_start),
        vaddr_file_end - vaddr_page_start,
        flags,
        MAP_PRIVATE | MAP_FIXED,
        exec_fd,
        phdr->p_offset & ~page_mask);

    if (vaddr_file_aligned_end != vaddr_page_end)
    {
        mmap_wrapper(
            PTR(vaddr_file_aligned_end),
            vaddr_page_end - vaddr_file_aligned_end,
            flags,
            MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS,
            -1,
            0);
    }

    SYSCALL(mimmutable(
        PTR(vaddr_page_start),
        vaddr_page_end - vaddr_page_start));

    return PTR(vaddr_page_start);
}

void mprotect_phdr(uintptr_t base, size_t page_size, int additional, elf32_phdr_t* phdr)
{
    int flags = mmap_flags_get(phdr->p_flags | additional);
    uintptr_t page_mask = page_size - 1;
    uintptr_t vaddr_start = base + phdr->p_vaddr;
    uintptr_t vaddr_page_start = vaddr_start & ~page_mask;
    uintptr_t vaddr_page_end = ALIGN_TO(vaddr_start + phdr->p_memsz, page_size);

    if (additional & PF_W)
    {
        flags &= ~PROT_EXEC;
    }

    SYSCALL(mprotect(
        PTR(vaddr_page_start),
        vaddr_page_end - vaddr_page_start,
        flags));
}

void mimmutable_phdr(uintptr_t base, size_t page_size, elf32_phdr_t* phdr)
{
    uintptr_t page_mask = page_size - 1;
    uintptr_t vaddr_start = base + phdr->p_vaddr;
    uintptr_t vaddr_page_start = vaddr_start & ~page_mask;
    uintptr_t vaddr_page_end = ALIGN_TO(vaddr_start + phdr->p_memsz, page_size);

    SYSCALL(mimmutable(
        PTR(vaddr_page_start),
        vaddr_page_end - vaddr_page_start));
}

const char* strtab_read(strtab_t* this, uintptr_t addr)
{
    return this->strings + addr;
}
