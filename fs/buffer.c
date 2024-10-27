#define log_fmt(fmt) "buffer: " fmt
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/page_alloc.h>

#define DEBUG_BUFFER 0

static LIST_DECLARE(buffers);

buffer_t* block_read(dev_t dev, file_t* file, uint32_t block)
{
    int res, errno;
    page_t* page;
    buffer_t* t;
    buffer_t* b[PAGE_SIZE / BLOCK_SIZE];
    size_t pages = block / (PAGE_SIZE / BLOCK_SIZE);
    size_t offset = block % (PAGE_SIZE / BLOCK_SIZE);

    log_debug(DEBUG_BUFFER, "reading block %u; page = %u, offset = %u", block, pages, offset);

    list_for_each_entry(t, &buffers, entry)
    {
        if (t->dev == dev && t->block == block)
        {
            log_debug(DEBUG_BUFFER, "found existing buffer");
            return t;
        }
    }

    log_debug(DEBUG_BUFFER, "reading buffer");

    page = page_alloc1();

    if (unlikely(!page))
    {
        log_warning("cannot allocate page for buffer");
        return ptr(-ENOMEM);
    }

    memset(page_virt_ptr(page), 0, PAGE_SIZE);

    for (int i = 0; i < PAGE_SIZE / BLOCK_SIZE; ++i)
    {
        b[i] = alloc(buffer_t);

        if (unlikely(!b[i]))
        {
            log_warning("no mem for buf %u", i);
            return ptr(-ENOMEM);
        }

        list_init(&b[i]->entry);
        list_init(&b[i]->buf_in_page);
        b[i]->block = pages * (PAGE_SIZE / BLOCK_SIZE) + i;
        b[i]->page = page;
        b[i]->dev = dev;
        b[i]->count = 1;
        b[i]->data = (char*)page_virt_ptr(page) + i * BLOCK_SIZE;

        list_add_tail(&b[i]->entry, &buffers);
    }

    file->offset = pages * PAGE_SIZE;
    res = file->ops->read(file, page_virt_ptr(page), PAGE_SIZE);

    if ((errno = errno_get(res)))
    {
        log_warning("read failed with %d", res);
        return ptr(errno);
    }

    ASSERT(offset < 4);

    return b[offset];
}
