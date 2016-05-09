#include <kernel/fs.h>
#include <kernel/kernel.h>

LIST_DECLARE(mounted_inodes);
struct inode *root;
