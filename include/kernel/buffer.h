#ifndef __BUFFER_H
#define __BUFFER_H

/*
 * Implementation of circular buffer of chars
 */

struct buffer {
    unsigned short in, out;
    unsigned short buffer_size;
    char *data;
};

#define DECLARE_BUFFER(name, size) \
    char __buffer_##name[size]; \
    struct buffer name = {      \
            0, 0, size,         \
            __buffer_##name     \
    }

/*===========================================================================*
 *                               buffer_empty                                *
 *===========================================================================*/
static inline int buffer_empty(struct buffer *buffer) {
    return (buffer->in == buffer->out);
}

void buffer_put(struct buffer *buffer, char c);
int buffer_get(struct buffer *buffer, char *c);

#endif
