#include <kernel/buffer.h>

void buffer_put(struct buffer* buffer, char c)
{
    buffer->data[buffer->in++] = c;
    if (buffer->in == buffer->buffer_size)
    {
        buffer->in = 0;
    }
}

int buffer_get(struct buffer* buffer, char* c)
{
    int ret = 1;
    if (buffer_empty(buffer))
    {
        goto finish;
    }

    *c = buffer->data[buffer->out++];
    if (buffer->out == buffer->buffer_size)
    {
        buffer->out = 0;
    }
    ret = 0;

finish:
    return ret;
}
