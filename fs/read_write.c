#include "kernel/page.h"
#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/process.h>
#include <kernel/api/unistd.h>

enum
{
    _READ,
    _WRITE,
};

static int access_init(int fd, const char* buffer, size_t size, int type, file_t** file)
{
    int errno;

    if (unlikely(errno = current_vm_verify_array(type == _READ ? VERIFY_WRITE : VERIFY_READ, buffer, size)))
    {
        return errno;
    }

    if (unlikely(fd_check_bounds(fd))) return -EBADF;

    process_fd_get(process_current, fd, file);

    if (unlikely(!*file)) return -EBADF;
    if (unlikely(!(*file)->ops)) return -ENOSYS;
    if (unlikely(type == _READ ? !(*file)->ops->read : !(*file)->ops->write)) return -ENOSYS;
    if (unlikely(((*file)->mode & O_ACCMODE) == (type == _READ ? O_WRONLY : O_RDONLY))) return -EACCES;

    return 0;
}

int sys_write(int fd, const char* buffer, size_t size)
{
    int errno;
    file_t* file;

    if (unlikely(errno = access_init(fd, buffer, size, _WRITE, &file)))
    {
        return errno;
    }

    return file->ops->write(file, (char*)buffer, size);
}

int sys_pwrite(int fd, const void* buffer, size_t size, off_t offset)
{
    int errno;
    file_t* file;

    if (unlikely(errno = access_init(fd, buffer, size, _WRITE, &file)))
    {
        return errno;
    }

    file->offset = offset;

    return file->ops->write(file, (char*)buffer, size);
}

int sys_read(int fd, char* buffer, size_t size)
{
    int errno;
    file_t* file;

    if (unlikely(errno = access_init(fd, buffer, size, _READ, &file)))
    {
        return errno;
    }

    return file->ops->read(file, buffer, size);
}

int sys_pread(int fd, void* buffer, size_t size, off_t offset)
{
    int errno;
    file_t* file;

    if (unlikely(errno = access_init(fd, buffer, size, _READ, &file)))
    {
        return errno;
    }

    file->offset = offset;

    return file->ops->read(file, buffer, size);
}

int do_read(file_t* file, size_t offset, void* buffer, size_t count)
{
    int errno;
    int retval;

    file->offset = offset;
    retval = file->ops->read(file, buffer, count);

    if (unlikely(errno = errno_get(retval)))
    {
        return errno;
    }

    return 0;
}

int do_write(file_t* file, size_t offset, const void* buffer, size_t count)
{
    int errno;
    int retval;

    file->offset = offset;
    retval = file->ops->write(file, buffer, count);

    if (unlikely(errno = errno_get(retval)))
    {
        return errno;
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
