#include <kernel/fs.h>
#include <kernel/kernel.h>

LIST_DECLARE(mounted_inodes);
LIST_DECLARE(free_inodes);
inode_t* root;

int inode_get(inode_t** inode)
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

    memset(*inode, 0, sizeof(inode_t));

    list_init(&(*inode)->list);

#ifdef USE_MAGIC
    (*inode)->magic = INODE_MAGIC;
#endif
    return 0;
}

int inode_put(inode_t* inode)
{
    list_del(&inode->list);
    list_add_tail(&inode->list, &free_inodes);
    return 0;
}

char* inode_print(const void* data, char* str)
{
    inode_t* inode = (inode_t*)data;
    str += sprintf(str, "inode_t{addr=%x, fs_data=%O}", (uint32_t)inode, inode->fs_data);
    return str;
}
