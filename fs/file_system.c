#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <arch/segment.h>

LIST_DECLARE(file_systems);
LIST_DECLARE(mounted_systems);

int vfs_init() {
    return 0;
}

void fullpath_get(const char *path, char *full_path) {

    strcpy(full_path, path);

}

int file_system_register(struct file_system *fs) {

    if (!fs->read_super || !fs->name) return -ENXIO;
    if (fs->file_systems.next) return -EBUSY;

    list_init(&fs->super_blocks);

    list_add_tail(&fs->file_systems, &file_systems);

    return 0;

}

int file_system_unregister(struct file_system *fs) {

    if (!list_empty(&fs->super_blocks)) return -EBUSY;

    list_del(&fs->file_systems);

    return 0;

}

int file_system_get(const char *name, struct file_system **fs) {

    struct file_system *temp;

    list_for_each_entry(temp, &file_systems, file_systems)
        if (!strcmp(name, temp->name)) {
            *fs = temp;
            return 0;
        }

    *fs = 0;
    return 1;

}

int do_mount(struct file_system *fs, const char *mount_point) {

    struct super_block *sb;
    struct mounted_system *ms;
    struct inode *temp_inode;
    char fullpath[255];

    if (CONSTRUCT(temp_inode))
        return -ENOMEM;

    fullpath_get(mount_point, fullpath);

    if (CONSTRUCT(sb, memset(sb, 0, sizeof(struct super_block))))
        return -ENOMEM;

    if (CONSTRUCT(ms, memset(ms, 0, sizeof(struct mounted_system))))
        return -ENOMEM;

    list_init(&ms->mounted_systems);
    list_init(&sb->super_blocks);

    fs->read_super(sb, 0, 0);
    ms->dir = kmalloc(255);
    strcpy(ms->dir, mount_point);
    ms->sb = sb;

    list_add_tail(&ms->mounted_systems, &mounted_systems);
    list_add_tail(&fs->super_blocks, &sb->super_blocks);

    if (!root) {
        if (strcmp(fullpath, "/")) return -ENODEV;
        sb->ops->read_inode(temp_inode);
        root = temp_inode;
        return 0;
    }

    temp_inode = lookup(mount_point);

    return 0;

}

int sys_mount(const char *source, const char *target,
        const char *filesystemtype, unsigned long mountflags) {

    struct file_system *fs;

    (void)source; (void)mountflags;

    if (file_system_get(filesystemtype, &fs))
        return -ENODEV;

    return do_mount(fs, target);

}

