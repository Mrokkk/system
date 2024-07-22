#include <stdarg.h>
#include <kernel/fs.h>
#include <kernel/process.h>

int do_ioctl(file_t* file, unsigned long request, void* arg)
{
    if (unlikely(!file->ops || !file->ops->ioctl))
    {
        return -ENOSYS;
    }

    return file->ops->ioctl(file, request, arg);
}

int sys_ioctl(int fd, unsigned long request, ...)
{
    va_list args;
    file_t* file;
    void* arg;

    if (unlikely(fd_check_bounds(fd) || process_fd_get(process_current, fd, &file)))
    {
        return -EBADF;
    }

    va_start(args, request);
    arg = va_arg(args, void*);
    va_end(args);

    return do_ioctl(file, request, arg);
}
