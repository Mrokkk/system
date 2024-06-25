#pragma once

#include <kernel/fs.h>
#include <kernel/binary.h>
#include <kernel/module.h>
#include <kernel/api/elf.h>

void elf_register(void);
int elf_module_load(const char* name, file_t* file, kmod_t** module);
