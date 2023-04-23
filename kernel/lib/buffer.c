#include <kernel/buffer.h>

#define buffer_empty(buffer) \
    ((buffer)->in == (buffer)->out)

void __buffer_put(struct buffer* buffer, char c)
{
    buffer->data[buffer->in++] = c;
    if (buffer->in == buffer->size)
    {
        buffer->in = 0;
    }
}

int __buffer_get(struct buffer* buffer, char* c)
{
    int ret = 1;
    if (buffer_empty(buffer))
    {
        goto finish;
    }

    *c = buffer->data[buffer->out++];
    if (buffer->out == buffer->size)
    {
        buffer->out = 0;
    }
    ret = 0;

finish:
    return ret;
}
