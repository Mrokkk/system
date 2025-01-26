#pragma once

#include <errno.h>
#include <stddef.h>
#include <syslog.h>
#include <unistd.h>

typedef int (*entry_t)();

#define DEBUG(fmt, ...) \
    ({ if (UNLIKELY(debug)) print(LOG_INFO, fmt, __VA_ARGS__); 0; })

#define SYSCALL(call) \
    ({ typeof(call) __v = call; if (UNLIKELY((int)__v == -1)) { die_errno(#call); }; __v; })

#define ALLOC(call) \
    ({ typeof(call) __v = call; if (UNLIKELY(!__v)) { errno = ENOMEM; die_errno(#call); }; __v; })

extern int debug;

void print_init(void);
void print(int error, const char* fmt, ...);
[[noreturn]] void die(const char* fmt, ...);
[[noreturn]] void die_errno(const char* string);

void read_from(int fd, void* buffer, size_t size, size_t offset);
void* alloc_read(int fd, size_t size, size_t offset);
