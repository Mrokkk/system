#include <kernel/fs.h>
#include <kernel/dev.h>
#include <kernel/fifo.h>
#include <kernel/dentry.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/api/stat.h>

#define DEBUG_PIPE 0

static int pipe_read(file_t* file, char* buffer, size_t count);
static int pipe_write(file_t* file, const char* buffer, size_t count);
static int pipe_r_close(file_t* file);
static int pipe_w_close(file_t* file);

typedef struct pipe pipe_t;

struct pipe
{
    wait_queue_head_t wq;
    int               writers;
    int               readers;
    int               count;
    BUFFER_MEMBER_DECLARE(buffer, 256);
};

static file_operations_t pipe_r_fops = {
    .read = &pipe_read,
    .close = &pipe_r_close,
};

static file_operations_t pipe_w_fops = {
    .write = &pipe_write,
    .close = &pipe_w_close,
};

static inode_operations_t pipe_iops = {
};

static file_operations_t pipe_fops = {
};

static super_operations_t sb_ops;

static super_block_t sb = {
    .ops = &sb_ops
};

static int pipe_read(file_t* file, char* buffer, size_t count)
{
    flags_t flags;
    size_t read;
    int errno;
    pipe_t* pipe = file->private;

    for (read = 0; read < count; ++read)
    {
        irq_save(flags);

        while (fifo_get(&pipe->buffer, &buffer[read]))
        {
            if (!pipe->writers)
            {
                log_debug(DEBUG_PIPE, "%u: pipe %p: no more writers", process_current->pid, pipe);
                irq_restore(flags);
                goto finish;
            }

            log_debug(DEBUG_PIPE, "%u: pipe %p: waiting", process_current->pid, pipe);
            WAIT_QUEUE_DECLARE(q, process_current);
            if ((errno = process_wait_locked(&pipe->wq, &q, &flags)))
            {
                irq_restore(flags);
                return errno;
            }
            log_debug(DEBUG_PIPE, "%u: pipe %p: woken", process_current->pid, pipe);
            irq_restore(flags);
        }
    }

finish:
    log_debug(DEBUG_PIPE, "%u: pipe: %p: read %u B", process_current->pid, pipe, read);
    return read;
}

static int pipe_write(file_t* file, const char* buffer, size_t count)
{
    flags_t flags;
    pipe_t* pipe = file->private;

    irq_save(flags);

    for (size_t i = 0; i < count; ++i)
    {
        fifo_put(&pipe->buffer, buffer[i]);
    }

    irq_restore(flags);

    process_t* proc = wait_queue_pop(&pipe->wq);

    log_debug(DEBUG_PIPE, "%u: pipe %p: written %u B", process_current->pid, pipe, count);

    if (proc)
    {
        log_debug_continue(DEBUG_PIPE, "; waking %u", proc->pid);
        process_wake(proc);
    }

    return count;
}

static int pipe_r_close(file_t* file)
{
    pipe_t* pipe = file->private;
    log_debug(DEBUG_PIPE, "%u: pipe %p: closing; refcount %u", process_current->pid, pipe, pipe->count);
    --pipe->readers;
    if (!--pipe->count)
    {
        log_debug(DEBUG_PIPE, "%u: pipe %p: removing", process_current->pid, pipe);
        delete(pipe);
    }
    dentry_delete(file->dentry);
    return 0;
}

static int pipe_w_close(file_t* file)
{
    pipe_t* pipe = file->private;
    log_debug(DEBUG_PIPE, "%u: pipe %p: closing; refcount %u", process_current->pid, pipe, pipe->count);
    if (!--pipe->writers)
    {
        process_t* proc = wait_queue_pop(&pipe->wq);

        if (proc)
        {
            log_debug_continue(DEBUG_PIPE, "; waking %u", proc->pid);
            process_wake(proc);
        }
    }
    if (!--pipe->count)
    {
        log_debug(DEBUG_PIPE, "%u: pipe %p: removing", process_current->pid, pipe);
        delete(pipe);
    }
    dentry_delete(file->dentry);
    return 0;
}

static void file_init(file_t* file, pipe_t* pipe, dentry_t* dentry, file_operations_t* fops, int mode)
{
    memset(file, 0, sizeof(*file));

    file->private = pipe;
    file->count = 1;
    file->ops = fops;
    file->mode = mode;
    file->dentry = dentry;
    pipe->count++;

    list_add_tail(&file->files, &files);
}

static void pipe_init(pipe_t* pipe)
{
    fifo_init(&pipe->buffer);
    wait_queue_head_init(&pipe->wq);
    pipe->count = 0;
    pipe->readers = 1;
    pipe->writers = 1;
}

static void inode_init(inode_t* inode)
{
    inode->ops = &pipe_iops;
    inode->file_ops = &pipe_fops;
    inode->dev = MKDEV(0, 255);
    inode->ino = 1;
    inode->sb = &sb;
    inode->mode = S_IFIFO | 666;
    inode->fs_data = NULL;
}

int sys_pipe(int* pipefd)
{
    int errno;
    file_t* input = NULL;
    file_t* output = NULL;
    pipe_t* pipe = NULL;
    int inputfd = -1, outputfd = -1;
    inode_t* inode_r;
    inode_t* inode_w;
    dentry_t* dentry_r;
    dentry_t* dentry_w;

    static unsigned id;

    if (process_find_free_fd(process_current, &inputfd))
    {
        return -EMFILE;
    }

    // FIXME: make fds a bitset and allocate it in process_find_free_fd
    process_fd_set(process_current, inputfd, (file_t*)1);

    if (unlikely(
        (errno = inode_alloc(&inode_r)) ||
        (errno = inode_alloc(&inode_w))))
    {
        goto error;
    }

    inode_init(inode_r);
    inode_init(inode_w);

    char namebuf_r[16];
    char namebuf_w[16];

    snprintf(namebuf_r, sizeof(namebuf_r), "pipe:%u", id++);
    snprintf(namebuf_w, sizeof(namebuf_w), "pipe:%u", id++);

    if (!(dentry_r = dentry_create(inode_r, NULL, namebuf_r)) ||
        !(dentry_w = dentry_create(inode_w, NULL, namebuf_w)))
    {
        errno = -EINVAL;
        goto error;
    }

    if (process_find_free_fd(process_current, &outputfd))
    {
        errno = -EMFILE;
        goto error;
    }

    if (!(pipe = alloc(pipe_t, pipe_init(this))) ||
        !(input = alloc(file_t, file_init(this, pipe, dentry_r, &pipe_r_fops, O_RDONLY))) ||
        !(output = alloc(file_t, file_init(this, pipe, dentry_w, &pipe_w_fops, O_WRONLY))))
    {
        errno = -ENOMEM;
        goto error;
    }

    process_fd_set(process_current, inputfd, input);
    process_fd_set(process_current, outputfd, output);

    pipefd[0] = inputfd;
    pipefd[1] = outputfd;

    return 0;

error:
    inputfd != -1 ? process_fd_set(process_current, inputfd, 0) : 0;
    outputfd != -1 ? process_fd_set(process_current, outputfd, 0) : 0;

    input ? do_close(input) : 0;
    output ? do_close(output) : 0;

    if (!input && pipe)
    {
        delete(pipe);
    }

    if (inode_r)
    {
        inode_put(inode_r);
    }

    if (inode_w)
    {
        inode_put(inode_w);
    }

    return errno;
}

int pipefs_init(void)
{
    return 0;
}
