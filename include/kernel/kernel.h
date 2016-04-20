#ifndef __KERNEL_H_
#define __KERNEL_H_

#ifdef __cplusplus
extern "C" {
#endif

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

struct kernel_init {
    char *name;
    int (*init)();
    int priority;
};

#define KERNEL_INIT(name, prio) \
        int name();                                 \
        __attribute__((section(".kernel_init")))    \
        struct kernel_init __##name = {             \
                #name, name, prio                   \
        };                                          \
        int name

#define INIT_PRIORITY_HI    0
#define INIT_PRIORITY_MED   1
#define INIT_PRIORITY_LO    2

struct kernel_symbol *symbol_find(const char *name);
int symbols_read(char *symbols, unsigned int size);

extern struct cpu_info cpu_info;
extern volatile unsigned int jiffies;
extern char *symbols;
extern unsigned int symbols_size;
extern unsigned int ram;

#define USER(x) __attribute__((section(".user_text"))) x

#ifdef __cplusplus
}
#endif

#endif
