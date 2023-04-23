#pragma once

#include <kernel/compiler.h>

// Implementation of circular buffer of chars

struct buffer
{
    unsigned short in, out;
    unsigned short size;
    char data[0];
};

#define BUFFER_MEMBER_DECLARE(name, size) \
    union \
    { \
        struct buffer __buf; \
        char __data[sizeof(struct buffer) + size]; \
    } name

#define BUFFER_DECLARE(name, size) \
    BUFFER_MEMBER_DECLARE(name, size) = { \
        {0, 0, size, }, \
    }

void __buffer_put(struct buffer* buffer, char c);
int __buffer_get(struct buffer* buffer, char* c);

#define buffer_init(name) \
    { \
        typecheck_ptr(name); \
        struct buffer* b = &(name)->__buf; \
        b->in = 0; b->out = 0; \
        b->size = sizeof((name)->__data) - sizeof(struct buffer); \
    }

#define buffer_put(b, c) \
    ({ typecheck_ptr(b); __buffer_put((struct buffer*)(b), c); })

#define buffer_get(b, c) \
    ({ typecheck_ptr(b); __buffer_get((struct buffer*)(b), c); })
