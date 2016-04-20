#include <kernel/buffer.h>
#include <kernel/kernel.h>

#define cast_buffer ((struct buffer_struct *)buffer)

/*===========================================================================*
 *                                buffer_init                                *
 *===========================================================================*/
int buffer_init(void *buffer, int size) {

    cast_buffer->in = 0;
    cast_buffer->out = 0;
    cast_buffer->buffer_size = size;
    cast_buffer->data = kmalloc(size);
    if (!cast_buffer->data) return -ENOMEM;

    return 0;

}

/*===========================================================================*
 *                                 buffer_put                                *
 *===========================================================================*/
int buffer_put(void *buffer, char c) {
    cast_buffer->data[(unsigned int)cast_buffer->in] = c;
    cast_buffer->in++;
    if (cast_buffer->in == cast_buffer->buffer_size)
        cast_buffer->in = 0;
    return 0;
}

/*===========================================================================*
 *                                 buffer_get                                *
 *===========================================================================*/
int buffer_get(void *buffer, char *c) {
    if (buffer_empty(cast_buffer)) {
        return 1;
    }
    *c = cast_buffer->data[(unsigned int)cast_buffer->out];
    cast_buffer->out++;
    if (cast_buffer->out == cast_buffer->buffer_size)
        cast_buffer->out = 0;
    return 0;
}

/*===========================================================================*
 *                               buffer_empty                                *
 *===========================================================================*/
int buffer_empty(void *buffer) {
    if (cast_buffer->in == cast_buffer->out)
        return 1;
    return 0;
}
