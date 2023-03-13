#include <kernel/fs.h>
#include <kernel/kernel.h>

LIST_DECLARE(files);
LIST_DECLARE(mounted_inodes);
struct inode *root;

void inode_init(inode_t* inode)
{
    memset(inode, 0, sizeof(inode_t));
#ifdef USE_MAGIC
    inode->magic = INODE_MAGIC;
#endif
}

char* inode_print(const void* data, char* str)
{
    inode_t* inode = (inode_t*)data;
    str += sprintf(str, "inode_t{addr=%x, fs_data=%O}", (uint32_t)inode, inode->fs_data);
    return str;
}
