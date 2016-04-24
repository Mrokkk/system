#include <kernel/buffer.h>
#include <kernel/kernel.h>

/*===========================================================================*
 *                                 buffer_put                                *
 *===========================================================================*/
void buffer_put(struct buffer *buffer, char c) {

    buffer->data[buffer->in++] = c;
    if (buffer->in == buffer->buffer_size)
        buffer->in = 0;

}

/*===========================================================================*
 *                                 buffer_get                                *
 *===========================================================================*/
int buffer_get(struct buffer *buffer, char *c) {

    if (buffer_empty(buffer))
        return 1;

    *c = buffer->data[buffer->out++];
    if (buffer->out == buffer->buffer_size)
        buffer->out = 0;
    return 0;

}

