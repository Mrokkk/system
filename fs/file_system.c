#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <arch/segment.h>

LIST_DECLARE(file_systems);
LIST_DECLARE(inodes);
struct inode *inode_hash_table[1024];

int vfs_init() {
    return 0;
}

int root_mount(const char *dev_name) {

    (void)dev_name;

    return -1;

}

int file_system_register(struct file_system *fs) {

    struct file_system *temp;

    list_for_each_entry(temp, &file_systems, file_systems)
        if (temp == fs) return -EEXIST;
    list_add(&fs->file_systems, &file_systems);

    return 0;

}

int sys_mount(const char *source, const char *target, const char *fs_type, int mount_flags, void *data) {
    (void)source; (void)target; (void)fs_type;
    (void)mount_flags; (void)data;
    return -1;
}

