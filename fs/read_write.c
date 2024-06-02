#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/unistd.h>
#include <kernel/process.h>

int sys_write(int fd, const char* buffer, size_t size)
{
    int errno;
    file_t* file;

    if ((errno = vm_verify(VERIFY_READ, buffer, size, process_current->mm->vm_areas)))
    {
        return errno;
    }

    if (fd_check_bounds(fd)) return -EBADF;

    process_fd_get(process_current, fd, &file);

    if (!file) return -EBADF;
    if (!file->ops) return -ENOSYS;
    if (!file->ops->write) return -ENOSYS;
    if ((file->mode & O_ACCMODE) == O_RDONLY) return -EACCES;

    return file->ops->write(file, (char*)buffer, size);
}

int sys_read(int fd, char* buffer, size_t size)
{
    int errno;
    file_t* file;

    if ((errno = vm_verify(VERIFY_WRITE, buffer, size, process_current->mm->vm_areas)))
    {
        return errno;
    }

    if (fd_check_bounds(fd)) return -EBADF;

    process_fd_get(process_current, fd, &file);

    if (!file) return -EBADF;
    if (!file->ops) return -ENOSYS;
    if (!file->ops->read) return -ENOSYS;
    if ((file->mode & O_ACCMODE) == O_WRONLY) return -EACCES;

    return file->ops->read(file, buffer, size);
}

int do_read(file_t* file, size_t offset, void* buffer, size_t count)
{
    int retval;

    file->offset = offset;
    retval = file->ops->read(file, buffer, count);

    if (retval < 0 && retval >= -ERRNO_MAX)
    {
        return retval;
    }

    // FIXME: this is wrong; such situation is normal
    if ((size_t)retval != count)
    {
        log_warning("retval = %d, count = %u", retval, count);
        return -EIO;
    }

    return 0;
}

int sys_lseek(int fd, off_t offset, int whence)
{
    file_t* file;

    if (whence > SEEK_END || whence < SEEK_SET)
    {
        return -EINVAL;
    }

    if (fd_check_bounds(fd)) return -EBADF;
    if (process_fd_get(process_current, fd, &file)) return -EBADF;

    // TODO: add checking whether offset can be set
    switch (whence)
    {
        case SEEK_SET:
            file->offset = offset;
            break;
        case SEEK_CUR:
            file->offset += offset;
            break;
        case SEEK_END:
            file->offset = file->inode->size + offset;
            break;
    }

    return 0;
}
