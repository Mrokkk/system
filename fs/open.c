#include <kernel/fs.h>
#include <kernel/path.h>
#include <kernel/stat.h>
#include <kernel/dentry.h>
#include <kernel/process.h>

int do_open(file_t** new_file, const char* filename, int flags, int mode)
{
    struct inode* inode = NULL;
    struct inode* parent_inode = NULL;
    dentry_t* parent_dentry = NULL;
    char parent[256];

    dirname(filename, parent);

    log_debug("calling lookup for %S; full filename: %S", parent, filename);
    parent_dentry = lookup(parent);

    if (!parent_dentry)
    {
        log_debug("no dentry for parent", filename);
        return -EBADF;
    }

    parent_inode = parent_dentry->inode;
    const char* basename = find_last_slash(filename) + 1;

    log_debug("calling lookup 2 with %s", basename);

    if (!strcmp(filename, parent))
    {
        inode = parent_inode;
        goto set_file;
    }

    int errno = parent_inode->ops->lookup(parent_inode, basename, 0, &inode);

    if (errno)
    {
        log_debug("failed lookup; calling ops->create");

        if (!(flags & O_CREAT))
        {
            log_debug("invalid mode");
            return -ENOENT;
        }

        errno = parent_inode->ops->create(parent_inode, basename, flags, mode, &inode);

        if (errno)
        {
            log_debug("create failed");
            return errno;
        }
    }

    if (!inode)
    {
        log_debug("inode is %O", inode);
        return -EERR;
    }

    if ((*new_file = alloc(file_t, list_add_tail(&this->files, &files))) == NULL)
    {
        log_debug("not enough memory");
        return -ENOMEM;
    }

set_file:
    log_debug("parent=%S, file=%O, inode=%O", parent, new_file, inode);

    (*new_file)->inode = inode;
    (*new_file)->ops = inode->file_ops;
    (*new_file)->mode = mode;

    if (!(*new_file)->ops)
    {
        log_debug("file ops is NULL for %S", filename);
        return -ENOOPS;
    }

    if (!(*new_file)->ops->open) return -ENOOPS;
    (*new_file)->ops->open(*new_file);

    return 0;
}

int sys_open(const char* user_filename, int flags, int mode)
{
    int fd, errno;
    struct file *file;

    if ((errno = path_validate(user_filename)))
    {
        log_debug("invalid path");
        return errno;
    }

    log_debug("with %S", user_filename);

    const char* filename = user_filename;
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

    log_debug("fd = %d", fd);

    return fd;
}

int sys_dup(int fd)
{
    int new_fd;
    file_t* file;

    if (fd_check_bounds(fd)) return -EMFILE;
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
        delete(file, list_del(&file->files));
    }
    return 0;
}

int sys_dup2(int oldfd, int newfd)
{
    int errno;
    file_t* oldfile;
    file_t* newfile;

    if (fd_check_bounds(oldfd) || fd_check_bounds(newfd)) return -EMFILE;
    if (process_fd_get(process_current, oldfd, &oldfile)) return -EBADF;
    if (process_fd_get(process_current, newfd, &newfile)) return -EBADF;

    if ((errno = do_close(oldfile)))
    {
        return errno;
    }

    process_fd_set(process_current, oldfd, newfile);
    newfile->count++;

    return 0;
}

int sys_close(int fd)
{
    file_t* file;

    if (fd_check_bounds(fd)) return -EMFILE;
    if (process_fd_get(process_current, fd, &file)) return -EBADF;
    process_fd_set(process_current, fd, 0);

    return do_close(file);
}

int sys_mkdir(const char* path, int mode)
{
    int errno;
    inode_t* inode;
    dentry_t* new_dentry;
    dentry_t* parent_dentry;
    char dir_name[64];
    const char* basename;

    // TODO: add address checking

    if ((errno = path_validate(path)))
    {
        log_debug("invalid path");
        return errno;
    }

    log_debug("path=%S", path);

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
        log_debug("cannot find dentry for parent: %s", path);
        return -EMFILE;
    }

    if (!parent_dentry->inode->ops || !parent_dentry->inode->ops->mkdir)
    {
        printk("No ops assigned to inode\n");
        return -EBADF;
    }

    errno = parent_dentry->inode->ops->mkdir(
        parent_dentry->inode,
        basename,
        mode,
        0,
        &inode);

    if (errno)
    {
        log_debug("error in ops->mkdir");
        return errno;
    }

    ASSERT(inode);
    new_dentry = dentry_create(inode, parent_dentry, basename);
    ASSERT(new_dentry);
    return 0;
}

int sys_creat(const char* pathname, int mode)
{
    log_debug("called with %S, %d", pathname, mode);
    // TODO: add address checking
    return sys_open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

int sys_getdents(unsigned int fd, struct dirent* dirp, size_t count)
{
    file_t *file;

    // TODO: add address checking

    log_debug("called with %d, %x, %u", fd, dirp, count);

    if (fd_check_bounds(fd)) return -EMFILE;
    if (process_fd_get(process_current, fd, &file)) return -EBADF;

    log_debug("file=%O ops=%O readdir=%O", file, file->ops, file->ops->readdir);

    return file->ops->readdir(file, dirp, count);
}

int sys_chdir(const char* path)
{
    dentry_t* dentry;

    // TODO: add address checking

    if ((dentry = lookup(path)) == NULL)
    {
        return -ENOENT;
    }
    process_current->fs->cwd = dentry;
    return 0;
}

int sys_getcwd(char* buf, size_t size)
{
    // TODO: add address checking

    if (!process_current->fs->cwd)
    {
        return -EACCES; // FIXME: find better errno
    }

    return construct_path(process_current->fs->cwd, buf, size);
}

int sys_stat(const char* pathname, struct stat* statbuf)
{
    dentry_t* dentry;
    if ((dentry = lookup(pathname)) == NULL)
    {
        return -ENOENT;
    }
    statbuf->st_ino = dentry->inode->ino;
    statbuf->st_dev = dentry->inode->dev;
    statbuf->st_size = dentry->inode->size;
    return 0;
}
