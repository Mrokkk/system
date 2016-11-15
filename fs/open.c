#include <kernel/fs.h>

static inline char *path_next(char *path) {

    return strchr(path, '/');

}

void base_name(char *path, char *name) {

    while (*path != '/' && *path != 0)
        *name++ = *path++;

    *name = 0;

}

struct inode *lookup(const char *filename) {

    char *path = (char *)filename;
    char name[255];
    struct inode *temp, *temp2;

    if (!root) return 0;

    if (*path == '/') path = path_next(path) + 1;
    temp = root;

    new(temp2); /* FIXME: Not freeing */

    while (1) {
        base_name(path, name);
        temp->ops->lookup(temp, name, 0, &temp2);
        temp = temp2;
        path = path_next(path);
        if (!path) break;
        path++;
    }

    return temp2;

}

int do_open(struct file **new_file, const char *filename, int mode) {

    struct inode *inode;
    (void) mode;

    inode = lookup(filename);

    if (new(*new_file, list_add_tail(&(*new_file)->files, &files)))
        return -ENOMEM;

    (*new_file)->inode = inode;
    (*new_file)->ops = inode->ops->file_ops;
    if (!(*new_file)->ops) {
        printk("File ops is NULL!\n");
        return -ENODEV;
    }
    if (!(*new_file)->ops->open) return -ENOOPS;
    (*new_file)->ops->open(inode, *new_file);

    return 0;

}

int sys_open(const char *filename, int mode) {

    int fd, errno;
    struct file *file;

    if (process_find_free_fd(process_current, &fd))
        return fd;
    if ((errno = do_open(&file, filename, mode)))
        return errno;
    process_fd_set(process_current, fd, file);
    file->ref_count = 1; // FIXME: what if two processes open the same file?

    return 0;

}

int sys_dup(int fd) {

    int new_fd;
    struct file *file;

    if (fd_check_bounds(fd))
        return -EMFILE;
    if (process_find_free_fd(process_current, &new_fd))
        return -ENOMEM;
    if (process_fd_get(process_current, fd, &file))
        return -EBADF;
    process_fd_set(process_current, new_fd, file);
    file->ref_count++;

    return 0;

}

int sys_close(int fd) {

    struct file *file;

    if (fd_check_bounds(fd))
        return -EMFILE;
    if (process_fd_get(process_current, fd, &file))
        return -EBADF;
    process_fd_set(process_current, fd, 0);

    if (!--file->ref_count)
       delete(file, list_del(&file->files));

    return 0;

}

