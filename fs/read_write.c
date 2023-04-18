#include <arch/vm.h>
#include <kernel/fs.h>
#include <kernel/process.h>

int sys_write(int fd, const char* buffer, size_t size)
{
    file_t* file;

#if 0 // FIXME: signals vma are not in mm->vm_areas
    int errno;
    if ((errno = vm_verify(VERIFY_READ, buffer, size, process_current->mm->vm_areas)))
    {
        return errno;
    }
#endif

    if (fd_check_bounds(fd)) return -EMFILE;

    process_fd_get(process_current, fd, &file);

    if (!file) return -EBADF;
    if (!file->ops) return -ENODEV;
    if (!file->ops->write) return -ENODEV;

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

    if (fd_check_bounds(fd)) return -EMFILE;

    process_fd_get(process_current, fd, &file);

    if (!file) return -EBADF;
    if (!file->ops) return -ENODEV;
    if (!file->ops->read) return -ENODEV;

    return file->ops->read(file, buffer, size);
}

