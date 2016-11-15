#include <kernel/kernel.h>
#include <kernel/malloc.h>
#include <kernel/string.h>

#include <arch/page.h>

extern unsigned int ram;
static char *heap;
LIST_DECLARE(memory_blocks);

#define BLOCK_FREE 1
#define BLOCK_BUSY 0

static inline void *ksbrk(int incr) {

    void *prev_heap;
    unsigned int last_page, new_page, diff, i;
    static struct memory_block *temp;

    if (!heap)
        heap = prev_heap = page_alloc();

    prev_heap = heap;

    last_page = (unsigned int)prev_heap / 4096;
    new_page = ((unsigned int)prev_heap + incr) / 4096;

    diff = new_page - last_page;

    (void)temp;

    if (diff) {
        unsigned int size = new_page * 0x1000 - (unsigned)prev_heap;
        if (size > MEMORY_BLOCK_SIZE) {
            temp = prev_heap;
            temp->free = 1;
            temp->size = size - MEMORY_BLOCK_SIZE;
            list_init(&temp->blocks);
            list_add_tail(&temp->blocks, &memory_blocks);
        }
        heap = prev_heap = page_alloc();
        for (i = 1; i < diff; i++)
            heap = page_alloc();
    }

    heap += incr;

    return prev_heap;

}

static struct memory_block *kmalloc_create_block(int size) {

    struct memory_block *new;

    if (!(new = (struct memory_block *)ksbrk(MEMORY_BLOCK_SIZE + size)))
        return new; /* We're out of memory */
    new->size = size;
    new->free = 0;
    list_init(&new->blocks);

    return new;

}

void *kmalloc(size_t size) {

    struct memory_block *temp;
    struct memory_block *new;

    ASSERT(size <= PAGE_SIZE);

    /* Align size to multiplication of MEMORY_BLOCK_SIZE */
    if (size % MEMORY_BLOCK_SIZE)
        size = (size / MEMORY_BLOCK_SIZE) * MEMORY_BLOCK_SIZE + MEMORY_BLOCK_SIZE;

    list_for_each_entry(temp, &memory_blocks, blocks) {

        /* If we have found block with bigger size */
        if (temp->free && temp->size >= size) {

            unsigned int old_size = temp->size;

            /* Try to divide it */
            if (old_size <= size + 2*MEMORY_BLOCK_SIZE) {
                temp->free = 0;
            } else {
                temp->free = 0;
                temp->size = size;
                new = (struct memory_block *)
                    ((unsigned int)temp->block_ptr + size);
                new->size = old_size - MEMORY_BLOCK_SIZE - temp->size;
                new->free = 1;
                list_add(&new->blocks, &temp->blocks);
            }

            return (void *)temp->block_ptr;
        }
    }

    /* Add next block to the end of the list */
    if (!(new = kmalloc_create_block(size))) return 0;
    list_add_tail(&new->blocks, &memory_blocks);

    return (void *)new->block_ptr;

}

int kfree(void *address) {

    struct memory_block *temp;

    list_for_each_entry(temp, &memory_blocks, blocks) {

        /* If given address matches the block data pointer, set free flag... */
        if ((unsigned int)temp->block_ptr == (unsigned int)address) {
            temp->free = 1;
            return 0; /* ...and return */
        }

#if 0
        if (temp->blocks.next != &memory_blocks) {

            struct memory_block *next =
                list_entry(temp->blocks.next, struct memory_block, blocks);

            /* Merge two neighboring blocks if they're free */
            if (next->free && temp->free) {
                temp->size = temp->size + next->size + MEMORY_BLOCK_SIZE;
                list_del(&next->blocks);
            }

        }
#endif

    }

    printk("cannot free 0x%x\n", (unsigned int)address);

    return -ENXIO;

}

