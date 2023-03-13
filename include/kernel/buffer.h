#pragma once

// Implementation of circular buffer of chars

struct buffer
{
    unsigned short in, out;
    unsigned short buffer_size;
    char* data;
};

#define BUFFER_DECLARE(name, size) \
    char __buffer_##name[size]; \
    struct buffer name = { \
            0, 0, size, \
            __buffer_##name \
    }

#define buffer_empty(buffer) \
    ((buffer)->in == (buffer)->out)

void buffer_put(struct buffer* buffer, char c);
int buffer_get(struct buffer* buffer, char* c);
