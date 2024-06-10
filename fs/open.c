#include <kernel/fs.h>
#include <kernel/path.h>
#include <kernel/stat.h>
#include <kernel/dentry.h>
#include <kernel/process.h>
#include <kernel/statvfs.h>

LIST_DECLARE(files);

int do_open(file_t** new_file, const char* filename, int flags, int mode)
{
    int errno;
    inode_t* inode = NULL;
    inode_t* parent_inode = NULL;
    dentry_t* parent_dentry = NULL;
    const char* basename;
    char parent[256];

    dentry_t* dentry = lookup(filename);

    if (dentry)
    {
        inode = dentry->inode;
        goto set_file;
    }
    else if (!dentry && !(flags & O_CREAT))
    {
        log_debug(DEBUG_OPEN, "invalid mode");
        return -ENOENT;
    }
    else
    {
        if (path_is_absolute(filename))
        {
            dirname(filename, parent);

            log_debug(DEBUG_OPEN, "calling lookup for %S; full filename: %S", parent, filename);
            parent_dentry = lookup(parent);

            if (!parent_dentry)
            {
                log_debug(DEBUG_OPEN, "no dentry for parent", filename);
                return -ENOENT;
            }
            basename = find_last_slash(filename) + 1;
        }
        else
        {
            parent_dentry = process_current->fs->cwd;
            basename = filename;
        }
    }

    parent_inode = parent_dentry->inode;

    if ((errno = parent_inode->ops->create(parent_inode, basename, flags, mode, &inode)))
    {
        log_debug(DEBUG_OPEN, "create failed");
        return errno;
    }

    if (unlikely(!inode))
    {
        log_error("inode is %O", inode);
        return -ENOENT;
    }

set_file:
    if (!inode->file_ops || !inode->file_ops->open)
    {
        return -ENOSYS;
    }

    if (S_ISDIR(inode->mode) && !(flags & O_DIRECTORY))
    {
        return -EISDIR;
    }

    // TODO: check permissions

    if (!(*new_file = alloc(file_t, list_add_tail(&this->files, &files))))
    {
        return -ENOMEM;
    }

    log_debug(DEBUG_OPEN, "file=%O, inode=%O", *new_file, inode);

    (*new_file)->inode = inode;
    (*new_file)->ops = inode->file_ops;
    (*new_file)->mode = flags;
    (*new_file)->offset = 0;
    (*new_file)->count = 1;
    (*new_file)->private = NULL;

    if ((errno = (*new_file)->ops->open(*new_file)))
    {
        list_del(&(*new_file)->files);
        delete(*new_file);
        *new_file = NULL;
        return errno;
    }

    return errno;
}

int sys_open(const char* __user filename, int flags, int mode)
{
    int fd, errno;
    struct file *file;

    if ((errno = path_validate(filename)))
    {
        return errno;
    }

    log_debug(DEBUG_OPEN, "with %S", filename);

    if (process_find_free_fd(process_current, &fd))
    {
        return -ENOMEM;
    }
    if ((errno = do_open(&file, filename, flags, mode)))
    {
        return errno;
    }

    process_fd_set(process_current, fd, file);
    file->count = 1;

    return fd;
}

int sys_dup(int fd)
{
    int new_fd;
    file_t* file;

    if (fd_check_bounds(fd)) return -EBADF;
    if (process_find_free_fd(process_current, &new_fd)) return -ENOMEM;
    if (process_fd_get(process_current, fd, &file)) return -EBADF;
    process_fd_set(process_current, new_fd, file);
    file->count++;

    return 0;
}

int do_close(file_t* file)
{
    if (!--file->count)
    {
        if (file->ops->close)
        {
            file->ops->close(file);
        }
        delete(file, list_del(&file->files));
    }
    return 0;
}

int sys_dup2(int oldfd, int newfd)
{
    file_t* oldfile;
    file_t* newfile;

    if (fd_check_bounds(oldfd) || fd_check_bounds(newfd)) return -EBADF;
    if (process_fd_get(process_current, oldfd, &oldfile)) return -EBADF;

    if (!process_fd_get(process_current, newfd, &newfile))
    {
        do_close(newfile);
    }

    process_fd_set(process_current, newfd, oldfile);
    oldfile->count++;

    return newfd;
}

int sys_close(int fd)
{
    file_t* file;

    if (fd_check_bounds(fd)) return -EBADF;
    if (process_fd_get(process_current, fd, &file)) return -EBADF;
    process_fd_set(process_current, fd, 0);

    return do_close(file);
}

int sys_mkdir(const char* __user path, int mode)
{
    int errno;
    inode_t* inode;
    dentry_t* new_dentry;
    dentry_t* parent_dentry;
    char dir_name[64];
    const char* basename;

    if ((errno = path_validate(path)))
    {
        return errno;
    }

    if (path_is_absolute(path))
    {
        dirname(path, dir_name);
        basename = find_last_slash(path) + 1;
        parent_dentry = lookup(dir_name);
    }
    else
    {
        basename = path;
        parent_dentry = process_current->fs->cwd;
    }

    if (!parent_dentry)
    {
        return -ENOENT;
    }

    if (!parent_dentry->inode->ops || !parent_dentry->inode->ops->mkdir)
    {
        return -ENOSYS;
    }

    errno = parent_dentry->inode->ops->mkdir(
        parent_dentry->inode,
        basename,
        mode,
        0,
        &inode);

    if (errno)
    {
        return errno;
    }

    ASSERT(inode);

    new_dentry = dentry_create(inode, parent_dentry, basename);

    if (!new_dentry)
    {
        return -ENOMEM;
    }

    return 0;
}

int sys_creat(const char* pathname, int mode)
{
    int errno;

    if ((errno = path_validate(pathname)))
    {
        return errno;
    }

    log_debug(DEBUG_OPEN, "called with %S, %d", pathname, mode);

    return sys_open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

int sys_chdir(const char* __user path)
{
    int errno;
    dentry_t* dentry;
    mode_t mode;

    if ((errno = path_validate(path)))
    {
        return errno;
    }

    if ((dentry = lookup(path)) == NULL)
    {
        return -ENOENT;
    }

    mode = dentry->inode->mode;

    if (!S_ISDIR(mode))
    {
        return -ENOTDIR;
    }

    if (!(mode & S_IXUSR) && !(mode & S_IXGRP) && !(mode & S_IXOTH))
    {
        return -EACCES;
    }

    process_current->fs->cwd = dentry;
    return 0;
}

int sys_getcwd(char* __user buf, size_t size)
{
    int errno;

    if ((errno = vm_verify(VERIFY_READ, buf, size, process_current->mm->vm_areas)))
    {
        return errno;
    }

    if (!process_current->fs->cwd)
    {
        return -EACCES; // FIXME: find better errno
    }

    return path_construct(process_current->fs->cwd, buf, size);
}

static void stat_fill(struct stat* statbuf, const dentry_t* dentry)
{
    statbuf->st_ino = dentry->inode->ino;
    statbuf->st_dev = dentry->inode->dev;
    statbuf->st_size = dentry->inode->size;
    statbuf->st_mode = dentry->inode->mode;
    statbuf->st_uid = dentry->inode->uid;
    statbuf->st_gid = dentry->inode->gid;
}

int sys_stat(const char* __user pathname, struct stat* __user statbuf)
{
    dentry_t* dentry;

    if (!(dentry = lookup(pathname)))
    {
        return -ENOENT;
    }

    stat_fill(statbuf, dentry);

    return 0;
}

int sys_fstat(int fd, struct stat* statbuf)
{
    dentry_t* dentry;

    file_t* file;

    if (fd_check_bounds(fd)) return -EBADF;
    if (process_fd_get(process_current, fd, &file)) return -EBADF;

    if (!(dentry = dentry_get(file->inode)))
    {
        log_error("VFS issue: inode %x has no dentry", file->inode);
        return -ENOENT;
    }

    stat_fill(statbuf, dentry);

    return 0;
}

int sys_statvfs(const char* path, struct statvfs* buf)
{
    int errno;
    dentry_t* dentry;

    if ((errno = path_validate(path)))
    {
        return errno;
    }

    if ((dentry = lookup(path)) == NULL)
    {
        return -ENOENT;
    }

    // FIXME: add proper implementation
    memset(buf, 0, sizeof(*buf));

    return 0;
}

int sys_fstatvfs(int fd, struct statvfs* buf)
{
    file_t* file;

    if (fd_check_bounds(fd)) return -EBADF;
    if (process_fd_get(process_current, fd, &file)) return -EBADF;

    // FIXME: add proper implementation
    memset(buf, 0, sizeof(*buf));

    return 0;
}

int sys_unlink(const char* path)
{
    (void)path;
    return -ENOSYS;
}

mode_t sys_umask(mode_t cmask)
{
    (void)cmask;
    return -ENOSYS;
}

int sys_fchmod(int fd, mode_t)
{
    file_t* file;

    if (fd_check_bounds(fd)) return -EBADF;
    if (process_fd_get(process_current, fd, &file)) return -EBADF;

    return -ENOSYS;
}

int sys_access(const char* path, int amode)
{
    int errno;
    dentry_t* dentry;

    (void)amode;

    if ((errno = path_validate(path)))
    {
        return errno;
    }

    if ((dentry = lookup(path)) == NULL)
    {
        return -ENOENT;
    }

    return 0;
}

int sys_rename(const char* oldpath, const char* newpath)
{
    (void)oldpath; (void)newpath;
    return -ENOSYS;
}

int sys_mknod(const char* pathname, mode_t mode, dev_t dev)
{
    (void)pathname, (void)mode; (void)dev;
    return -ENOSYS;
}
