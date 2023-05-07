#pragma once

#include <kernel/compiler.h>

// Implementation of circular buffer of chars

struct fifo
{
    unsigned short in, out;
    unsigned short size;
    char data[0];
};

typedef struct fifo fifo_t;

#define BUFFER_MEMBER_DECLARE(name, size) \
    union \
    { \
        struct fifo __buf; \
        char __data[sizeof(struct fifo) + size]; \
    } name

#define BUFFER_DECLARE(name, size) \
    BUFFER_MEMBER_DECLARE(name, size) = { \
        {0, 0, size, }, \
    }

void __fifo_put(fifo_t* fifo, char c);
int __fifo_get(fifo_t* fifo, char* c);

#define fifo_init(name) \
    { \
        typecheck_ptr(name); \
        fifo_t* b = &(name)->__buf; \
        b->in = 0; b->out = 0; \
        b->size = sizeof((name)->__data) - sizeof(fifo_t); \
    }

#define fifo_put(b, c) \
    ({ typecheck_ptr(b); __fifo_put((fifo_t*)(b), c); })

#define fifo_get(b, c) \
    ({ typecheck_ptr(b); __fifo_get((fifo_t*)(b), c); })
