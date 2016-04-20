#include <kernel/fs.h>
#include <kernel/kernel.h>

struct inode *inodes[256][MAX_INODES];

int inode_put(struct inode *inode) {

    (void)inode;

    return 0;

}

