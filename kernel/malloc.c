#include <kernel/kernel.h>
#include <kernel/malloc.h>
#include <kernel/string.h>

static struct memory_block *kmalloc_create_block(int size);

extern unsigned int ram;
static char *heap = _end;
LIST_DECLARE(memory_blocks);

/*===========================================================================*
 *                                   ksbrk                                   *
 *===========================================================================*/
static inline void *ksbrk(int incr) {

    void *prev_heap;

    if (incr < 0) return 0;

    prev_heap = heap;

    /* Check if it wouldn't exceed available RAM */
    if ((unsigned int)prev_heap + incr >= ram + 0xc0000000) /* FIXME: */
        return 0;

    heap += incr;

    return prev_heap;

}

/*===========================================================================*
 *                            kmalloc_create_block                           *
 *===========================================================================*/
static struct memory_block *kmalloc_create_block(int size) {

    struct memory_block *new;

    if (!(new = (struct memory_block *)ksbrk(MEMORY_BLOCK_SIZE + size)))
        return new; /* We're out of memory */
    new->size = size;
    new->free = 0;
    list_init(&new->blocks);

    return new;

}

/*===========================================================================*
 *                                  kmalloc                                  *
 *===========================================================================*/
void *kmalloc(size_t size) {
    
    struct memory_block *temp;
    struct memory_block *new;

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

/*===========================================================================*
 *                                   kfree                                   *
 *===========================================================================*/
int kfree(void *address) {

    struct memory_block *temp;

    list_for_each_entry(temp, &memory_blocks, blocks) {

        /* If given address matches the block data pointer, set free flag... */
        if ((unsigned int)temp->block_ptr == (unsigned int)address) {
            temp->free = 1;
            return 0; /* ...and return */
        }

        if (temp->blocks.next != &memory_blocks) {

            struct memory_block *next =
                list_entry(temp->blocks.next, struct memory_block, blocks);

            /* Merge two neighboring blocks if they're free */
            if (next->free && temp->free) {
                temp->size = temp->size + next->size + MEMORY_BLOCK_SIZE;
                list_del(&next->blocks);
            }

        }

    }

    printk("cannot free 0x%x\n", (unsigned int)address);
    
    return -ENXIO;

}

