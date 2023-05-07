#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/stat.h>
#include <kernel/process.h>

struct readdir_data
{
    struct dirent* current;
    struct dirent* previous;
    int count;
    int errno;
};

int dirent_add(void* buf, const char* name, size_t len, ino_t ino, char type)
{
    struct readdir_data* data = buf;
    struct dirent* dirent = data->current;

    int dirent_size = align(len + sizeof(struct dirent) + 1, sizeof(long));

    if (dirent_size > data->count)
    {
        return 1;
    }

    dirent->ino = ino;
    dirent->len = dirent_size;
    dirent->type = type;
    strncpy(dirent->name, name, len);
    dirent->name[len] = 0;

    data->count -= dirent_size;
    data->previous = data->current;
    data->current = ptr(addr(data->current) + dirent_size);

    log_debug(DEBUG_READDIR, "added %S, len=%u", name, len);

    return 0;
}

int sys_getdents(unsigned int fd, void* dirp, size_t count)
{
    int errno;
    file_t *file;
    struct readdir_data data;

    if (unlikely((errno = vm_verify(VERIFY_WRITE, dirp, count, process_current->mm->vm_areas))))
    {
        log_debug(DEBUG_READDIR, "invalid ptr passed: %x", dirp);
        return errno;
    }

    if (unlikely(fd_check_bounds(fd)))
    {
        log_debug(DEBUG_READDIR, "fd outside of range: %u", fd);
        return -EMFILE;
    }

    if (unlikely(process_fd_get(process_current, fd, &file)))
    {
        log_debug(DEBUG_READDIR, "invalid fd: %u", fd);
        return -EBADF;
    }

    if (unlikely(!file->ops || !file->ops->readdir))
    {
        log_debug(DEBUG_READDIR, "no operation for file: %x", file);
        return -ENOSYS;
    }

    log_debug(DEBUG_READDIR, "file=%O ops=%O readdir=%O", file, file->ops, file->ops->readdir);

    data.current = dirp;
    data.previous = dirp;
    data.count = count;

    return file->ops->readdir(file, &data, &dirent_add);
}
