#include <kernel/fs.h>
#include <kernel/process.h>

int sys_write(int fd, const char* buffer, size_t size)
{
    file_t* file;
    process_fd_get(process_current, fd, &file);

    if (!file) return -EBADF;
    if (!file->ops) return -ENODEV;
    if (!file->ops->write) return -ENODEV;

    return file->ops->write(file, (char*)buffer, size);
}

int sys_read(int fd, char* buffer, size_t n)
{
    file_t* file;
    process_fd_get(process_current, fd, &file);

    if (!file) return -EBADF;
    if (!file->ops) return -ENODEV;
    if (!file->ops->read) return -ENODEV;

    return file->ops->read(file, buffer, n);
}

