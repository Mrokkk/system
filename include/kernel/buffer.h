#ifndef __BUFFER_H
#define __BUFFER_H

/*
 * Circular buffer implementation
 */

struct buffer_struct {
    char in, out;
    char buffer_size;
    char *data;
};

#define create_buffer_struct(name) \
    struct name {            \
        char in, out;        \
        char buffer_size;    \
        char *data;          \
    }

int buffer_init(void *buf, int size);
int buffer_put(void *buf, char c);
int buffer_get(void *buf, char *c);
int buffer_empty(void *buf);

#endif
