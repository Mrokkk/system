#define log_fmt(fmt) "file_system: " fmt
#include <kernel/fs.h>
#include <kernel/path.h>
#include <kernel/devfs.h>
#include <kernel/process.h>

static LIST_DECLARE(file_systems);
static LIST_DECLARE(mounted_systems);
static dentry_t* root_dentry;
static inode_t* root;

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

static int mount_impl(file_system_t* fs, const char* source, const char* mount_point, dev_t dev, file_t* file)
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

    lookup(mount_point, LOOKUP_NOFOLLOW, NULL, &dentry);

    if (dentry)
    {
        if (!S_ISDIR(dentry->inode->mode))
        {
            return -ENOTDIR;
        }

        log_info("mounting %s in %s on %s", fs->name, mount_point, source);

        errno = fs->mount(sb, dentry->inode, NULL, 0);

        inode = dentry->inode;
    }
    else if (unlikely(!root))
    {
        if (unlikely(errno = inode_alloc(&inode)))
        {
            log_error("failed to get root inode");
            return errno;
        }

        inode->mode = S_IFDIR | 0755;
        errno = fs->mount(sb, inode, NULL, 0);

        if (!errno)
        {
            log_info("setting root = %p", inode);
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
    inode->dev = dev;

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

int do_mount(const char* source, const char* target, const char* filesystemtype, unsigned long)
{
    static int last_minor = 1;
    int errno;
    dev_t dev = 0;
    file_system_t* fs;
    file_t* file = NULL;

    if (strcmp(source, "none"))
    {
        if (unlikely(errno = do_open(&file, source, O_RDWR, 0)))
        {
            goto error;
        }

        dev = file->dentry->inode->rdev;
    }
    else
    {
        dev = MKDEV(0, last_minor++);
    }

    if (unlikely(file_system_get(filesystemtype, &fs)))
    {
        errno = -ENODEV;
        goto error;
    }

    return mount_impl(fs, source, target, dev, file);

error:
    if (file)
    {
        delete(file);
    }
    return errno;
}

int sys_mount(const char* source, const char* target, const char* filesystemtype, unsigned long mountflags)
{
    int errno;

    if ((errno = path_validate(target)) ||
        (errno = current_vm_verify_string(VERIFY_READ, target)) ||
        (errno = current_vm_verify_string(VERIFY_READ, filesystemtype)))
    {
        return errno;
    }

    return do_mount(source, target, filesystemtype, mountflags);
}

int sys_umount(const char*)
{
    return -ENOSYS;
}

int sys_umount2(const char*, int)
{
    return -ENOSYS;
}

int do_chroot(const char* path)
{
    int errno;
    dentry_t* dentry;

    if (unlikely(!process_current->fs->root && !path))
    {
        process_current->fs->root = root_dentry;
        return 0;
    }

    if (unlikely(errno = lookup(path, LOOKUP_NOFOLLOW, NULL, &dentry)))
    {
        return errno;
    }

    if (!S_ISDIR(dentry->inode->mode))
    {
        return -ENOTDIR;
    }

    process_current->fs->root = dentry;

    return 0;
}

int sys_chroot(const char* path)
{
    int errno;

    if ((errno = path_validate(path)))
    {
        return errno;
    }

    return do_chroot(path);
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
