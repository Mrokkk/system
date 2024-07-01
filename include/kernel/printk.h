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

typedef struct printk_entry printk_entry_t;

struct file;

void printk_register(struct file* file);
void printk(const printk_entry_t* entry, const char*, ...);
void panic(const char* fmt, ...);

void ensure_printk_will_print(void);
void printk_early_register(void (*print)(const char* string));

#define PRINTK_ENTRY(name, severity) \
    struct printk_entry name = { \
        .log_level  = *severity, \
        .line       = __LINE__, \
        .file       = __builtin_strrchr(__FILE__, '/') + 1, \
        .function   = __FUNCTION__, \
    }

#define __log(fmt, ...) \
    ({ \
        PRINTK_ENTRY(__e, fmt); \
        printk(&__e, &fmt[KERN_LEVEL_SIZE], ##__VA_ARGS__); \
    })

#define log(sev, fmt, ...) \
    ({ \
        __log(sev fmt, ##__VA_ARGS__); \
    })

#define log_severity(sev, fmt, ...) \
    ({ \
        PRINTK_ENTRY(__e, sev); \
        printk(&__e, fmt, ##__VA_ARGS__); \
    })

#ifndef log_fmt
#define log_fmt(fmt) fmt
#endif

#define log_debug(flag, fmt, ...)          flag ? log(KERN_DEBUG, fmt, ##__VA_ARGS__) : 0
#define log_info(fmt, ...)                 log(KERN_INFO,   log_fmt(fmt),   ##__VA_ARGS__)
#define log_notice(fmt, ...)               log(KERN_NOTICE, log_fmt(fmt),   ##__VA_ARGS__)
#define log_warning(fmt, ...)              log(KERN_WARN,   log_fmt(fmt),   ##__VA_ARGS__)
#define log_error(fmt, ...)                log(KERN_ERR,    log_fmt(fmt),   ##__VA_ARGS__)
#define log_exception(fmt, ...)            log(KERN_ERR,    log_fmt(fmt),   ##__VA_ARGS__)
#define log_critical(fmt, ...)             log(KERN_CRIT,   log_fmt(fmt),   ##__VA_ARGS__)
#define log_continue(fmt, ...)             log(KERN_CONT,   fmt,            ##__VA_ARGS__)
#define log_debug_continue(flag, fmt, ...) flag ? log(KERN_CONT, fmt, ##__VA_ARGS__) : 0

#define PROCESS_FMT        "%s[%u]: "
#define PROCESS_PARAMS(p)  (p)->name, (p)->pid

#define process_log(severity_name, fmt, proc, ...) \
    log_##severity_name(PROCESS_FMT fmt, PROCESS_PARAMS(proc), ##__VA_ARGS__)

#define process_log_debug(flag, fmt, proc, ...)  log_debug(flag, PROCESS_FMT fmt, PROCESS_PARAMS(proc), ##__VA_ARGS__)
#define process_log_info(fmt, proc, ...)         process_log(info, fmt, proc, ##__VA_ARGS__)
#define process_log_notice(fmt, proc, ...)       process_log(notice, fmt, proc, ##__VA_ARGS__)
#define process_log_warning(fmt, proc, ...)      process_log(warning, fmt, proc, ##__VA_ARGS__)
#define process_log_error(fmt, proc, ...)        process_log(error, fmt, proc, ##__VA_ARGS__)
#define process_log_exception(fmt, proc, ...)    log_exception(exception, fmt, proc, ##__VA_ARGS__)
#define process_log_critical(fmt, proc, ...)     log_critical(PROCESS_FMT fmt, proc, ##__VA_ARGS__)

#define current_log_debug(flag, fmt, ...)  log_debug(flag, PROCESS_FMT fmt, PROCESS_PARAMS(process_current), ##__VA_ARGS__)
#define current_log_info(fmt, ...)         process_log_info(fmt, process_current, ##__VA_ARGS__)
#define current_log_notice(fmt, ...)       process_log_notice(fmt, process_current, ##__VA_ARGS__)
#define current_log_warning(fmt, ...)      process_log_warning(fmt, process_current, ##__VA_ARGS__)
#define current_log_error(fmt, ...)        process_log_error(fmt, process_current, ##__VA_ARGS__)
#define current_log_exception(fmt, ...)    process_log_exception(fmt, process_current, ##__VA_ARGS__)
#define current_log_critical(fmt, ...)     process_log_critical(fmt, process_current, ##__VA_ARGS__)

#define panic_if(condition, ...) \
    do \
    { \
        if (unlikely(condition)) \
        { \
            panic(__VA_ARGS__); \
        } \
    } \
    while (0)
