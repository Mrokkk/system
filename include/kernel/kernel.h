#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <arch/system.h>
#include <arch/processor.h>

#include <kernel/div.h>
#include <kernel/list.h>
#include <kernel/unit.h>
#include <kernel/debug.h>
#include <kernel/magic.h>
#include <kernel/debug.h>
#include <kernel/errno.h>
#include <kernel/limits.h>
#include <kernel/malloc.h>
#include <kernel/printf.h>
#include <kernel/printk.h>
#include <kernel/string.h>
#include <kernel/compiler.h>
#include <kernel/sections.h>
#include <kernel/api/types.h>

int strtoi(const char* str);

#define __user

void breakpoint(void);
