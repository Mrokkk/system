#include <kernel/fs.h>
#include <kernel/kernel.h>

LIST_DECLARE(file_systems);

int fs_init() {
    return 0;
}

void fullpath_get(const char *path, char *full_path) {

    strcpy(full_path, path);

}

int file_system_register(struct file_system *fs) {

    if (!fs->read_super || !fs->name) return -ENXIO;

    list_add_tail(&fs->file_systems, &file_systems);

    return 0;

}

struct file_system *file_system_get(const char *name) {

    struct file_system *fs;

    list_for_each_entry(fs, &file_systems, file_systems) {
        if (!strcmp(name, fs->name))
            return fs;
    }

    return 0;

}

int do_mount(struct file_system *fs, const char *mount_point) {

    struct super_block temp;
    struct inode *temp_inode;
    char fullpath[255];

    if (!(temp_inode = kmalloc(sizeof(struct inode))))
        return -ENOMEM;

    fullpath_get(mount_point, fullpath);

    if (strcmp(fullpath, "/")) return -ENODEV;

    fs->read_super(&temp, 0, 0);
    temp.ops->read_inode(temp_inode);

    root = temp_inode;

    return 0;

}

int sys_mount(const char *name, const char *mount_point) {

    char kname[255];
    char kmount[255];
    int i;
    struct file_system *fs;

    for (i = 0; i < 64; i++) {
        get_from_user(&kname[i], (void *)&name[i]);
        if (kname[i] == 0)
            break;
    }

    for (i = 0; i < 64; i++) {
        get_from_user(&kmount[i], (void *)&mount_point[i]);
        if (kmount[i] == 0)
            break;
    }

    if (!(fs = file_system_get(kname))) return -ENODEV;

    return do_mount(fs, kmount);

}

