#pragma once

#include <kernel/fs.h>
#include <kernel/api/mman.h>

MUST_CHECK(void*) do_mmap(void* addr, size_t len, int prot, int flags, file_t* file, size_t offset);
