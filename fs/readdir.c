#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/process.h>
#include <kernel/api/stat.h>

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

    strlcpy(dirent->d_name, name, len + 1);
    dirent->d_ino       = ino;
    dirent->d_off       = 0;
    dirent->d_reclen    = dirent_size;
    dirent->d_type      = type;
    dirent->d_name[len] = 0;

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

    if (unlikely((errno = current_vm_verify_buf(VERIFY_WRITE, dirp, count))))
    {
        log_debug(DEBUG_READDIR, "invalid ptr passed: %p", dirp);
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
        log_debug(DEBUG_READDIR, "no operation for file: %p", file);
        return -ENOSYS;
    }

    log_debug(DEBUG_READDIR, "file=%O ops=%O readdir=%O", file, file->ops, file->ops->readdir);

    data.current = dirp;
    data.previous = dirp;
    data.count = count;

    return file->ops->readdir(file, &data, &dirent_add);
}
