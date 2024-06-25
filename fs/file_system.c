#define log_fmt(fmt) "file_system: " fmt
#include <kernel/fs.h>
#include <kernel/path.h>
#include <kernel/devfs.h>
#include <kernel/process.h>

LIST_DECLARE(file_systems);
LIST_DECLARE(mounted_systems);

dentry_t* root_dentry;

void fullpath_get(const char* path, char* full_path)
{
    strcpy(full_path, path);
}

int file_system_register(file_system_t* fs)
{
    if (!fs->mount || !fs->name)
    {
        return -EINVAL;
    }

    if (fs->file_systems.next)
    {
        return -EBUSY;
    }

    list_init(&fs->super_blocks);

    list_add_tail(&fs->file_systems, &file_systems);

    return 0;
}

int file_system_unregister(file_system_t* fs)
{
    if (!list_empty(&fs->super_blocks))
    {
        return -EBUSY;
    }

    list_del(&fs->file_systems);

    return 0;
}

int file_system_get(const char* name, file_system_t** fs)
{
    file_system_t* temp;

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

int do_mount(file_system_t* fs, const char* source, const char* mount_point, dev_t dev, file_t* file)
{
    int errno;
    inode_t* inode = NULL;
    dentry_t* dentry;
    struct super_block* sb;
    struct mounted_system* ms;

    if (!(sb = alloc(super_block_t, memset(this, 0, sizeof(super_block_t)))))
    {
        return -ENOMEM;
    }

    if (!(ms = alloc(mounted_system_t, memset(this, 0, sizeof(mounted_system_t)))))
    {
        delete(sb);
        return -ENOMEM;
    }

    list_init(&ms->mounted_systems);
    list_init(&sb->super_blocks);

    sb->device_file = file;
    sb->dev = dev;
    inode = NULL;

    dentry = lookup(mount_point);

    if (dentry)
    {
        if (!S_ISDIR(dentry->inode->mode))
        {
            return -ENOTDIR;
        }

        log_info("mounting %s in inode = %x", fs->name, dentry->inode);

        errno = fs->mount(sb, dentry->inode, NULL, 0);
    }
    else if (unlikely(!root))
    {
        if (unlikely(errno = inode_get(&inode)))
        {
            log_error("failed to get root inode");
            return errno;
        }

        errno = fs->mount(sb, inode, NULL, 0);

        if (!errno)
        {
            log_info("setting root = %x", inode);
            root = inode;
            root_dentry = dentry_create(root, NULL, "/");
        }
        else
        {
            inode_put(inode);
        }
    }
    else
    {
        return -ENOENT;
    }

    if (unlikely(errno))
    {
        log_info("mount on %s returned %d", fs->name, errno);
        return errno;
    }

    sb->mounted = inode;

    ms->dir = slab_alloc(strlen(mount_point) + 1);
    ms->device = slab_alloc(strlen(source) + 1);
    ms->sb = sb;
    ms->fs = fs;
    strcpy(ms->dir, mount_point);
    strcpy(ms->device, source);

    list_add_tail(&ms->mounted_systems, &mounted_systems);
    list_add_tail(&fs->super_blocks, &sb->super_blocks);

    return 0;
}

int sys_mount(const char* source, const char* target, const char* filesystemtype, unsigned long mountflags)
{
    int errno;
    dev_t dev = 0;
    file_system_t* fs;
    file_t* file = NULL;
    inode_t* fake_inode = NULL;
    file_operations_t* ops;

    (void)mountflags;

    if (strcmp(source, "none"))
    {
        // FIXME: this is a bad workaround for lack of devfs while mounting root
        if (!root)
        {
            errno = inode_get(&fake_inode);

            if (unlikely(errno))
            {
                log_warning("cannot allocate inode");
                goto error;
            }

            if (!(ops = devfs_blk_get(strrchr(source, '/') + 1, &dev)))
            {
                errno = -ENODEV;
                goto error;
            }

            file = alloc(file_t);

            if (unlikely(!file))
            {
                errno = -ENOMEM;
                goto error;
            }

            memset(file, 0, sizeof(*file));
            file->ops = ops;
            file->inode = fake_inode;
            fake_inode->dev = dev;
        }
        else
        {
            if ((errno = do_open(&file, source, O_RDWR, 0)))
            {
                goto error;
            }
        }
    }

    if ((errno = path_validate(target)))
    {
        return errno;
    }

    if (file_system_get(filesystemtype, &fs))
    {
        errno = -ENODEV;
        goto error;
    }

    return do_mount(fs, source, target, dev, file);

error:
    if (file)
    {
        delete(file);
    }
    if (fake_inode)
    {
        inode_put(fake_inode);
    }
    return errno;
}

int sys_umount(const char*)
{
    return -ENOSYS;
}

int sys_umount2(const char*, int)
{
    return -ENOSYS;
}

int sys_chroot(const char* path)
{
    int errno;
    dentry_t* dentry;

    if ((errno = path_validate(path)))
    {
        return errno;
    }

    if (!process_current->fs->root)
    {
        process_current->fs->root = root_dentry;
        return 0;
    }

    if ((dentry = lookup(path)) == NULL)
    {
        return -ENOENT;
    }

    if (!S_ISDIR(dentry->inode->mode))
    {
        return -ENOTDIR;
    }

    process_current->fs->root = dentry;

    return 0;
}

int sys_statfs(const char* path, struct statfs* buf)
{
    UNUSED(path); UNUSED(buf);
    return -ENOSYS;
}

int sys_fstatfs(int fd, struct statfs* buf)
{
    UNUSED(fd); UNUSED(buf);
    return -ENOSYS;
}

void file_systems_print(void)
{
    mounted_system_t* ms;
    file_system_t* fs;
    log_info("mountpoints:");
    list_for_each_entry(ms, &mounted_systems, mounted_systems)
    {
        log_info("%s on %s type %s", ms->device, ms->dir, ms->fs->name);
    }
    log_info("registered file systems:");
    list_for_each_entry(fs, &file_systems, file_systems)
    {
        log_continue(" %s", fs->name);
    }
}
