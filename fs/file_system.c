#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <arch/segment.h>

LIST_DECLARE(file_systems);
LIST_DECLARE(inodes);
struct inode *inode_hash_table[1024];

int vfs_init() {
    return 0;
}

/*===========================================================================*
 *                            file_system_register                           *
 *===========================================================================*/
int file_system_register(struct file_system *fs) {

    // TODO: check if fs already exists
    list_add(&fs->file_systems, &file_systems);

    return 0;

}

/*===========================================================================*
 *                                 sys_mount                                 *
 *===========================================================================*/
int sys_mount(const char *source, const char *target, const char *fs_type, int mount_flags, void *data) {
    (void)source; (void)target; (void)fs_type;
    (void)mount_flags; (void)data;
    return 0;
}


