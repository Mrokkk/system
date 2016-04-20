#include <kernel/kernel.h>
#include <kernel/malloc.h>
#include <kernel/string.h>

static struct memory_block *kmalloc_create_block(int size);

extern unsigned int ram;
static char *heap = _end;
struct memory_block *memory_blocks = 0;

/*===========================================================================*
 *                                   ksbrk                                   *
 *===========================================================================*/
static inline void *ksbrk(int incr) {

    void *prev_heap;

    if (incr < 0) return 0;

    prev_heap = heap;

    /* Check if it wouldn't exceed available RAM */
    if ((unsigned int)prev_heap + incr >= ram) return 0;

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
    new->next = 0;

    return new;

}

/*===========================================================================*
 *                                  kmalloc                                  *
 *===========================================================================*/
void *kmalloc(size_t size) {
    
    struct memory_block *temp = memory_blocks;
    struct memory_block *new;

    /* Align size to multiplication of MEMORY_BLOCK_SIZE */
    if (size % MEMORY_BLOCK_SIZE)
        size = (size / MEMORY_BLOCK_SIZE) * MEMORY_BLOCK_SIZE + MEMORY_BLOCK_SIZE;

    /* If list doesn't exist, create it */
    if (unlikely(temp == NULL)) {
        if (!(new = kmalloc_create_block(size))) return 0;
        memory_blocks = new;
        return (void *)new->block_ptr;
    }

    for (; temp->next != NULL; temp = temp->next) {

        /* If we found block with bigger size */
        if (temp->free && temp->size >= size) {

            unsigned int old_size = temp->size;

            /* Try to divide it */
            if (old_size <= size + 2*MEMORY_BLOCK_SIZE) {
                temp->free = 0;
            } else {
                temp->free = 0;
                temp->size = size;
                new = (struct memory_block *)((unsigned int)temp->block_ptr + size);
                new->free = 1;
                new->next = temp->next;
                new->size = old_size - MEMORY_BLOCK_SIZE - temp->size;
                temp->next = new;
            }

            return (void *)temp->block_ptr;
        }
    }

    /* Add nex block to the end of the list */
    if (!(new = kmalloc_create_block(size))) return 0;
    temp->next = new;

    return (void *)new->block_ptr;

}

/*===========================================================================*
 *                                   kfree                                   *
 *===========================================================================*/
int kfree(void *address) {

    struct memory_block *temp = memory_blocks;

    do {

        /* If given address matches block data pointer, set free flag... */
        if ((unsigned int)temp->block_ptr == (unsigned int)address) {
            temp->free = 1;
            return 0; /* ...and return */
        }

        if (temp->next) {

            /* Merge two neighboring blocks if they're free */
            if (temp->next->free && temp->free) {
                temp->size = temp->size + temp->next->size + MEMORY_BLOCK_SIZE;
                temp->next = temp->next->next;
            }

        }

        temp = temp->next;

    } while (temp != NULL);

    printk("cannot free 0x%x\n", (unsigned int)address);
    
    return -ENXIO;

}

