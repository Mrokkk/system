#ifndef __KERNEL_H_
#define __KERNEL_H_

#include <kernel/version.h>
#include <kernel/compile.h>
#include <kernel/config.h>
#include <kernel/compiler.h>
#include <kernel/stddef.h>
#include <kernel/printk.h>
#include <kernel/sections.h>
#include <kernel/debug.h>
#include <kernel/types.h>
#include <kernel/malloc.h>
#include <kernel/errno.h>
#include <kernel/string.h>
#include <kernel/list.h>
#include <arch/processor.h>
#include <arch/system.h>

#define STACK_MAGIC 0xdeadc0de

#define __FILENAME__ (strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/') + 1) : __FILE__)

int printf(const char *, ...) __attribute__ ((format (printf, 1, 2))); /* TODO: remove */
int sprintf(char *, const char *, ...) __attribute__ ((format (printf, 2, 3)));
int vsprintf(char *, const char *, __builtin_va_list) __attribute__ ((format (printf, 2, 0)));
int fprintf(int fd, const char *fmt, ...);
long strtol(const char *, char **endptr, int);
int arch_info_get(char *buffer);

struct cpu_info {
    char vendor[16];
    char producer[16];
    int family;
    int model;
    char name[46];
    int stepping;
    unsigned int mhz;
    unsigned int cache;
    unsigned int bogomips;
    unsigned int features;
};

struct kernel_symbol {
    char name[32];
    unsigned int address;
    unsigned short size;
    char type;
};

struct kernel_symbol *symbol_find(const char *name);
struct kernel_symbol *symbol_find_address(unsigned int address);
int symbols_read(char *symbols, unsigned int size);
void arch_setup();

extern unsigned long init_kernel_stack[];
extern struct cpu_info cpu_info;
extern volatile unsigned int jiffies;
extern char *symbols;
extern unsigned int symbols_size;
extern unsigned int ram;

#endif
