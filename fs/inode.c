#include <kernel/fs.h>
#include <kernel/kernel.h>

struct inode *inodes[MAX_INODES];
LIST_DECLARE(mounted_inodes);
struct inode *root;
