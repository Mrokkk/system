#include <kernel/fifo.h>

#define fifo_empty(fifo) \
    ((fifo)->in == (fifo)->out)

void __fifo_put(fifo_t* fifo, char c)
{
    fifo->data[fifo->in++] = c;
    if (fifo->in == fifo->size)
    {
        fifo->in = 0;
    }
}

int __fifo_get(fifo_t* fifo, char* c)
{
    int ret = 1;
    if (fifo_empty(fifo))
    {
        goto finish;
    }

    *c = fifo->data[fifo->out++];
    if (fifo->out == fifo->size)
    {
        fifo->out = 0;
    }
    ret = 0;

finish:
    return ret;
}
