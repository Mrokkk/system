#include <kernel/fs.h>
#include <kernel/kernel.h>

struct inode *inodes[MAX_INODES];
LIST_DECLARE(mounted_inodes);
struct inode *root;

struct inode *inode_create() {

    struct inode *inode;

    if ((inode = kmalloc(sizeof(struct inode))) == 0) return 0;

    inodes[inode->ino] = inode;

    return 0;

}

