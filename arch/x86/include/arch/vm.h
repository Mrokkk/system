#pragma once

#include <kernel/page_table.h>
#include <kernel/page_types.h>

static inline pgprot_t vm_to_pgprot(const vm_area_t* vma)
{
    int vm_flags = vma->vm_flags;
    pgprot_t pg_flags = PAGE_PRESENT | PAGE_USER;
    if (vm_flags & VM_WRITE) pg_flags |= PAGE_RW;
    if (vm_flags & VM_IO) pg_flags |= PAGE_PCD;
    return pg_flags;
}
