#pragma once

#define KERN_DEBUG      "\0"
#define KERN_INFO       "\1"
#define KERN_NOTICE     "\2"
#define KERN_WARN       "\3"
#define KERN_ERR        "\4"
#define KERN_CRIT       "\5"

#define LOGLEVEL_DEBUG  0
#define LOGLEVEL_INFO   1
#define LOGLEVEL_NOTICE 2
#define LOGLEVEL_WARN   3
#define LOGLEVEL_ERR    4
#define LOGLEVEL_CRIT   5

struct printk_entry
{
    const int log_level;
    const int line;
    const char* file;
    const char* function;
};

struct file;

void printk_register(struct file* file);
int __printk(struct printk_entry* entry, const char*, ...);
void panic(const char* fmt, ...);

void ensure_printk_will_print(void);

#define printk(fmt, ...) \
    ({ \
        struct printk_entry e = { \
            .log_level = *fmt, \
            .line = __LINE__, \
            .file = __FILE__, \
            .function = __FUNCTION__, \
        }; \
        __printk(&e, &fmt[1], ##__VA_ARGS__); \
    })

#define log_debug(flag, ...) { if (flag) printk(KERN_DEBUG __VA_ARGS__); }
#define log_info(...) printk(KERN_INFO __VA_ARGS__)
#define log_notice(...) printk(KERN_NOTICE __VA_ARGS__)
#define log_warning(...) printk(KERN_WARN __VA_ARGS__)
#define log_error(...) printk(KERN_ERR __VA_ARGS__)
#define log_exception(...) printk(KERN_ERR __VA_ARGS__)
#define log_critical(...) printk(KERN_CRIT __VA_ARGS__)
#define log_trace(flag, ...) { if (flag) log_debug(__VA_ARGS__); }
