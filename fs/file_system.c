#include <kernel/fs.h>

LIST_DECLARE(file_systems);
LIST_DECLARE(mounted_systems);

int vfs_init()
{
    return 0;
}

void fullpath_get(const char* path, char* full_path)
{
    strcpy(full_path, path);
}

int file_system_register(struct file_system* fs)
{
    if (!fs->read_super || !fs->name)
    {
        return -ENXIO;
    }
    else if (fs->file_systems.next)
    {
        return -EBUSY;
    }

    list_init(&fs->super_blocks);

    list_add_tail(&fs->file_systems, &file_systems);

    return 0;
}

int file_system_unregister(struct file_system* fs)
{
    if (!list_empty(&fs->super_blocks))
    {
        return -EBUSY;
    }

    list_del(&fs->file_systems);

    return 0;
}

int file_system_get(const char* name, struct file_system** fs)
{
    struct file_system* temp;

    list_for_each_entry(temp, &file_systems, file_systems)
    {
        if (!strcmp(name, temp->name))
        {
            *fs = temp;
            return 0;
        }
    }

    *fs = 0;
    return 1;
}

int do_mount(struct file_system* fs, const char* mount_point)
{
    struct super_block* sb;
    struct mounted_system* ms;
    char fullpath[PATH_MAX];

    fullpath_get(mount_point, fullpath);

    if ((sb = alloc(struct super_block, memset(this, 0, sizeof(struct super_block)))) == NULL)
    {
        return -ENOMEM;
    }

    if ((ms = alloc(struct mounted_system, memset(this, 0, sizeof(struct mounted_system)))) == NULL)
    {
        return -ENOMEM;
    }

    list_init(&ms->mounted_systems);
    list_init(&sb->super_blocks);

    // TODO: return errno instead of sb ptr
    fs->read_super(sb, 0, 0);

    ms->dir = kmalloc(strlen(mount_point) + 1);
    strcpy(ms->dir, mount_point);
    ms->sb = sb;

    list_add_tail(&ms->mounted_systems, &mounted_systems);
    list_add_tail(&fs->super_blocks, &sb->super_blocks);

    // TODO: don't handle root differently
    if (!root)
    {
        if (strcmp(fullpath, "/"))
        {
            return -ENODEV;
        }

        if ((root = alloc(inode_t, inode_init(this))) == NULL)
        {
            return -ENOMEM;
        }

        sb->ops->read_inode(root);
        dentry_create(root, NULL, "/"); // FIXME:
        return 0;
    }

    dentry_t* dentry = lookup(mount_point);

    if (!dentry)
    {
        log_debug("cannot find %S", mount_point);
        return -ENOENT;
    }

    inode_t* inode = dentry->inode;

    if (!inode)
    {
        panic("no inode assigned");
    }

    inode->sb = sb;
    sb->ops->read_inode(inode);

    return 0;
}

int sys_mount(
    const char* source,
    const char* target,
    const char* filesystemtype,
    unsigned long mountflags)
{
    (void)source; (void)mountflags;

    struct file_system* fs;
    if (file_system_get(filesystemtype, &fs))
    {
        return -ENODEV;
    }

    return do_mount(fs, target);
}

void mountpoints_print()
{
    log_debug("mountpoints:");
}
