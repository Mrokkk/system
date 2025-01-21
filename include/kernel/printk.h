#pragma once

#include <kernel/compiler.h>

#define KERN_CONT       0
#define KERN_DEBUG      1
#define KERN_INFO       2
#define KERN_NOTICE     3
#define KERN_WARN       4
#define KERN_ERR        5
#define KERN_CRIT       6

#define PRINTK_FORMAT_CHECK 0

struct printk_entry
{
    const char  log_level;
    const int   line;
    const char* file;
    const char* function;
};

typedef int loglevel_t;
typedef unsigned long logseq_t;
typedef struct printk_entry printk_entry_t;

struct tty;
struct file;

logseq_t printk(const printk_entry_t* entry, const char*, ...)
#if PRINTK_FORMAT_CHECK
    __attribute__((format(printf, 2, 3)))
#endif
;
void NORETURN(panic(const char* fmt, ...));

void printk_register(struct tty* tty);
void ensure_printk_will_print(void);

#define PRINTK_ENTRY(name, loglevel) \
    const printk_entry_t name = { \
        .log_level  = loglevel, \
        .line       = __LINE__, \
        .file       = (loglevel) == KERN_DEBUG ? __builtin_strrchr(__FILE__, '/') + 1 : NULL, \
        .function   = (loglevel) == KERN_DEBUG ? __FUNCTION__ : NULL, \
    }

#define log(sev, fmt, ...) \
    ({ \
        PRINTK_ENTRY(__e, sev); \
        printk(&__e, fmt, ##__VA_ARGS__); \
    })

#ifndef log_fmt
#define log_fmt(fmt) fmt
#endif

#define log_debug(flag, fmt, ...)          flag ? log(KERN_DEBUG, fmt, ##__VA_ARGS__) : (void)0
#define log_info(fmt, ...)                 log(KERN_INFO,   log_fmt(fmt),   ##__VA_ARGS__)
#define log_notice(fmt, ...)               log(KERN_NOTICE, log_fmt(fmt),   ##__VA_ARGS__)
#define log_warning(fmt, ...)              log(KERN_WARN,   log_fmt(fmt),   ##__VA_ARGS__)
#define log_error(fmt, ...)                log(KERN_ERR,    log_fmt(fmt),   ##__VA_ARGS__)
#define log_exception(fmt, ...)            log(KERN_ERR,    log_fmt(fmt),   ##__VA_ARGS__)
#define log_critical(fmt, ...)             log(KERN_CRIT,   log_fmt(fmt),   ##__VA_ARGS__)
#define log_continue(fmt, ...)             log(KERN_CONT,   fmt,            ##__VA_ARGS__)
#define log_debug_continue(flag, fmt, ...) flag ? log(KERN_CONT, fmt, ##__VA_ARGS__) : (void)0

#define panic_if(condition, ...) \
    do \
    { \
        if (unlikely(condition)) \
        { \
            panic(__VA_ARGS__); \
        } \
    } \
    while (0)
