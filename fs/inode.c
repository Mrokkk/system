#include <kernel/fs.h>
#include <kernel/kernel.h>

static LIST_DECLARE(free_inodes);

int inode_alloc(inode_t** inode)
{
    if (list_empty(&free_inodes))
    {
        *inode = alloc(inode_t);
    }
    else
    {
        *inode = list_front(&free_inodes, inode_t, list);
        list_del(&(*inode)->list);
    }

    if (unlikely(!*inode))
    {
        return -ENOMEM;
    }

    memset(*inode, 0, sizeof(**inode));
    (*inode)->refcount = 1;

    list_init(&(*inode)->list);

    return 0;
}

int inode_get(inode_t* inode)
{
    ++inode->refcount;
    return 0;
}

void inode_put(inode_t* inode)
{
    if (!--inode->refcount)
    {
        list_del(&inode->list);
        list_add_tail(&inode->list, &free_inodes);
    }
}
