#include "kernel/page.h"
#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/process.h>
#include <kernel/api/unistd.h>

int sys_write(int fd, const char* buffer, size_t size)
{
    int errno;
    file_t* file;

    if ((errno = current_vm_verify_array(VERIFY_READ, buffer, size)))
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

    if ((errno = current_vm_verify_array(VERIFY_WRITE, buffer, size)))
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

int string_read(file_t* file, size_t offset, size_t count, string_t** output)
{
    int errno, retval;
    string_t* string;

    file->offset = offset;

    if (unlikely(!(string = string_create(count + 1))))
    {
        return -ENOMEM;
    }

    retval = file->ops->read(file, string->data, count);

    if (unlikely(errno = errno_get(retval)))
    {
        goto free_string;
    }

    string->len = retval;
    string->data[retval] = '\0';
    *output = string;

    return 0;

free_string:
    string_free(string);
    return errno;
}

int sys_lseek(int fd, off_t offset, int whence)
{
    file_t* file;

    if (whence > SEEK_END || whence < 0)
    {
        return -EINVAL;
    }

    if (fd_check_bounds(fd)) return -EBADF;
    if (process_fd_get(process_current, fd, &file)) return -EBADF;

    // TODO: add checking whether offset can be set
    switch (whence)
    {
        case 0: // FIXME: wtf, bash?
            file->offset = 0;
            break;
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

    return file->offset;
}
