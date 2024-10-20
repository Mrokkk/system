#pragma once

#include <arch/segment.h>
#include <kernel/linkage.h>
#include <kernel/compiler.h>

#ifdef __ASSEMBLER__
#define SECTION(sect) \
    .section sect

#define SEGMENT_REGISTERS_SET(value, scratch_register) \
    mov $value, %scratch_register; \
    mov %ax, %ds; \
    mov %ax, %es; \
    mov %ax, %fs; \
    mov %ax, %gs; \
    mov %ax, %ss

#else
#define ASM_VALUE(x) "$" STRINGIFY(x)
#endif

#ifdef __i386__
#include "asm_32.h"
#else
#include "asm_64.h"
#endif
