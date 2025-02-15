#pragma once

#include <arch/segment.h>
#include <kernel/linkage.h>
#include <kernel/compiler.h>

#ifdef __ASSEMBLER__
#define SECTION(sect) \
    .section sect

#define SEGMENT_REGISTERS_SET(value, scratch_register) \
    mov $value, %scratch_register; \
    mov %scratch_register, %ds; \
    mov %scratch_register, %es; \
    mov %scratch_register, %fs; \
    mov %scratch_register, %gs; \
    mov %scratch_register, %ss

#else
#define ASM_VALUE(x) "$" STRINGIFY(x)
#endif

#ifdef __i386__
#include "asm_32.h"
#else
#include "asm_64.h"
#endif
