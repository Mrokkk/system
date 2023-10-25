#pragma once

#define KERN_DEBUG      "\0"
#define KERN_INFO       "\1"
#define KERN_NOTICE     "\2"
#define KERN_WARN       "\3"
#define KERN_ERR        "\4"
#define KERN_CRIT       "\5"
#define KERN_CONT       "\6"
#define KERN_LEVEL_SIZE 1

#define LOGLEVEL_DEBUG  0
#define LOGLEVEL_INFO   1
#define LOGLEVEL_NOTICE 2
#define LOGLEVEL_WARN   3
#define LOGLEVEL_ERR    4
#define LOGLEVEL_CRIT   5
#define LOGLEVEL_CONT   6

struct printk_entry
{
    const char log_level;
    const int line;
    const char* file;
    const char* function;
};

struct file;

void printk_register(struct file* file);
void __printk(struct printk_entry* entry, const char*, ...);
void panic(const char* fmt, ...);

void ensure_printk_will_print(void);
void printk_early_register(void (*print)(const char* string));

#define printk(fmt, ...) \
    ({ \
        struct printk_entry __e = { \
            .log_level  = *fmt, \
            .line       = __LINE__, \
            .file       = __builtin_strrchr(__FILE__, '/') + 1, \
            .function   = __FUNCTION__, \
        }; \
        __printk(&__e, &fmt[KERN_LEVEL_SIZE], ##__VA_ARGS__); \
    })

#ifndef log_fmt
#define log_fmt(fmt) fmt
#endif

#define log_debug(flag, fmt, ...)   flag ? printk(KERN_DEBUG fmt, ##__VA_ARGS__) : 0
#define log_info(fmt, ...)          printk(KERN_INFO    log_fmt(fmt),   ##__VA_ARGS__)
#define log_notice(fmt, ...)        printk(KERN_NOTICE  log_fmt(fmt),   ##__VA_ARGS__)
#define log_warning(fmt, ...)       printk(KERN_WARN    log_fmt(fmt),   ##__VA_ARGS__)
#define log_error(fmt, ...)         printk(KERN_ERR     log_fmt(fmt),   ##__VA_ARGS__)
#define log_exception(fmt, ...)     printk(KERN_ERR     log_fmt(fmt),   ##__VA_ARGS__)
#define log_critical(fmt, ...)      printk(KERN_CRIT    log_fmt(fmt),   ##__VA_ARGS__)
#define log_continue(fmt, ...)      printk(KERN_CONT    fmt,            ##__VA_ARGS__)
