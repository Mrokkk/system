#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/time.h>
#include <kernel/wait.h>
#include <kernel/process.h>

int sys_select(int nfds, fd_set_t* readfds, fd_set_t* writefds, fd_set_t* exceptfds, timeval_t* timeout)
{
    NOT_USED(nfds);
    NOT_USED(readfds);
    NOT_USED(writefds);
    NOT_USED(exceptfds);
    NOT_USED(timeout);
    return -ENOSYS;
}

struct poll_data
{
    struct pollfd* fd;
    file_t* file;
    wait_queue_t queue;
    wait_queue_head_t* head;
};

typedef struct poll_data poll_data_t;

int sys_poll(struct pollfd* fds, unsigned long nfds, int timeout)
{
    int errno;
    flags_t flags;
    poll_data_t* data;
    file_t* file;
    int ready = 0;

    if ((errno = vm_verify(VERIFY_WRITE, fds, sizeof(struct pollfd) * nfds, process_current->mm->vm_areas)))
    {
        return errno;
    }

    if (timeout == 0)
    {
        return 0;
    }

    data = alloc_array(poll_data_t, nfds);

    if (unlikely(!data))
    {
        return -ENOMEM;
    }

    for (size_t i = 0; i < nfds; ++i)
    {
        fds[i].revents = 0;
        data[i].fd = &fds[i];

        if (process_fd_get(process_current, fds[i].fd, &data[i].file))
        {
            errno = -EBADF;
            goto error;
        }

        if (!data[i].file->ops || !data[i].file->ops->poll)
        {
            errno = -ENOSYS;
            goto error;
        }

        data->head = NULL;
        wait_queue_init(&data[i].queue, process_current);
    }

    while (!ready)
    {
        for (size_t i = 0; i < nfds; ++i)
        {
            file = data[i].file;
            if ((errno = file->ops->poll(file, data[i].fd->events, &data[i].fd->revents, &data[i].head)))
            {
                goto error;
            }

            if (!data[i].head)
            {
                ++ready;
            }
            else
            {
                wait_queue_push(&data[i].queue, data[i].head);
            }
        }

        if (!ready)
        {
            irq_save(flags);
            log_debug(DEBUG_POLL, "waiting for %u fds", nfds);
            process_wait2(flags);
        }

        for (size_t i = 0; i < nfds; ++i)
        {
            if (data[i].head)
            {
                wait_queue_remove(&data[i].queue, data[i].head);
            }
        }

        signal_run(process_current);

        if (!process_is_running(process_current))
        {
            // FIXME: I don't have any other idea for the moment; this makes sure that process won't
            // go to sleep (thus avoiding exit)
            delete_array(data, nfds);
            return -ERESTART;
        }
    }

    delete_array(data, nfds);

    return ready;

error:
    for (size_t i = 0; i < nfds; ++i)
    {
        if (data[i].head)
        {
            wait_queue_remove(&data[i].queue, data[i].head);
        }
    }

    if (data)
    {
        delete_array(data, nfds);
    }
    return errno;
}
