#include <kernel/fs.h>
#include <kernel/path.h>

LIST_DECLARE(file_systems);
LIST_DECLARE(mounted_systems);

void fullpath_get(const char* path, char* full_path)
{
    strcpy(full_path, path);
}

int file_system_register(struct file_system* fs)
{
    if (!fs->mount || !fs->name)
    {
        return -EINVAL;
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
    int errno;
    inode_t* inode = NULL;
    dentry_t* dentry;
    struct super_block* sb;
    struct mounted_system* ms;

    if (!(sb = alloc(struct super_block, memset(this, 0, sizeof(struct super_block)))))
    {
        return -ENOMEM;
    }

    if (!(ms = alloc(struct mounted_system, memset(this, 0, sizeof(struct mounted_system)))))
    {
        delete(sb);
        return -ENOMEM;
    }

    list_init(&ms->mounted_systems);
    list_init(&sb->super_blocks);

    sb->dev = 0; // TODO: add support for mounting with device
    inode = NULL;

    dentry = lookup(mount_point);

    if (dentry)
    {
        log_info("mounting %s in inode = %x", fs->name, dentry->inode);
        // TODO: check if it's a dir
        errno = fs->mount(sb, dentry->inode, NULL, 0);
    }
    else if (unlikely(!root))
    {
        if (unlikely(errno = inode_get(&inode)))
        {
            log_warning("failed to get root inode");
            return errno;
        }

        errno = fs->mount(sb, inode, NULL, 0);

        if (!errno)
        {
            log_info("setting root = %x", inode);
            root = inode;
            dentry_create(root, NULL, "/");
        }
    }
    else
    {
        return -ENOENT;
    }

    if (unlikely(errno))
    {
        log_warning("mount on %s returned %d", fs->name, errno);
        return errno;
    }

    sb->mounted = inode;

    ms->dir = slab_alloc(strlen(mount_point) + 1);
    ms->sb = sb;
    ms->fs = fs;
    strcpy(ms->dir, mount_point);

    list_add_tail(&ms->mounted_systems, &mounted_systems);
    list_add_tail(&fs->super_blocks, &sb->super_blocks);

    return 0;
}

int sys_mount(
    const char* source,
    const char* target,
    const char* filesystemtype,
    unsigned long mountflags)
{
    int errno;

    (void)source; (void)mountflags;

    if ((errno = path_validate(target)))
    {
        return errno;
    }

    struct file_system* fs;
    if (file_system_get(filesystemtype, &fs))
    {
        return -ENODEV;
    }

    return do_mount(fs, target);
}

void file_systems_print(void)
{
    struct mounted_system* ms;
    struct file_system* fs;
    log_info("mountpoints:");
    list_for_each_entry(ms, &mounted_systems, mounted_systems)
    {
        log_info("%s on %s", ms->fs->name, ms->dir);
    }
    log_info("registered file systems:");
    list_for_each_entry(fs, &file_systems, file_systems)
    {
        log_info("%s", fs->name);
    }
}
