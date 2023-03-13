#pragma once

// Some common used includes and functions

#include <stddef.h>
#include <stdint.h>

#include <arch/system.h>
#include <arch/processor.h>

#include <kernel/list.h>
#include <kernel/debug.h>
#include <kernel/errno.h>
#include <kernel/magic.h>
#include <kernel/trace.h>
#include <kernel/types.h>
#include <kernel/limits.h>
#include <kernel/malloc.h>
#include <kernel/printk.h>
#include <kernel/string.h>
#include <kernel/compiler.h>
#include <kernel/sections.h>

int sprintf(char*, const char*, ...);
int fprintf(int fd, const char* fmt, ...);
int vsprintf(char*, const char*, __builtin_va_list);

#define KiB (1024UL)
#define MiB (1024UL * KiB)
#define GiB (1024UL * MiB)

extern volatile unsigned int jiffies;
