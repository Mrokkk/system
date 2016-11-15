#include <kernel/process.h>
#include <kernel/fs.h>

int sys_write(int fd, const char *buffer, size_t size) {

    struct file *file;

    process_fd_get(process_current, fd, &file);

    if (!file) return -EBADF;
    if (!file->ops) return -ENODEV;
    if (!file->ops->write) return -ENODEV;

    return file->ops->write(file->inode, file, (char *)buffer, size);

}

int sys_read(int fd, char *buffer, size_t n) {

    struct file *file;

    process_fd_get(process_current, fd, &file);

    if (!file) return -EBADF;
    if (!file->ops) return -ENODEV;
    if (!file->ops->read) return -ENODEV;
    return file->ops->read(file->inode, file, buffer, n);

}

