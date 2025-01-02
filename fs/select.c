#include <stdbool.h>
#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/time.h>
#include <kernel/wait.h>
#include <kernel/timer.h>
#include <kernel/bitset.h>
#include <kernel/process.h>

struct poll_data
{
    struct pollfd* fd;
    file_t* file;
    wait_queue_t queue;
    wait_queue_head_t* head;
};

typedef struct poll_data poll_data_t;

static void do_poll_timeout(ktimer_t* timer)
{
    bool* timedout = timer->data;
    *timedout = true;
    process_wake(timer->process);
}

static int do_poll(struct pollfd* fds, const unsigned long nfds, timeval_t* timeout)
{
    int errno;
    flags_t flags;
    poll_data_t* data;
    file_t* file;
    int ready = 0;
    volatile bool timedout = false;

    if (timeout && timeout->tv_sec == 0 && timeout->tv_usec == 0)
    {
        return 0;
    }

    data = alloc_array(poll_data_t, nfds);

    if (unlikely(!data))
    {
        return -ENOMEM;
    }

    memset(data, 0, sizeof(*data) * nfds);

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

    if (timeout != NULL)
    {
        ktimer_create_and_start(KTIMER_ONESHOT, *timeout, &do_poll_timeout, (void*)&timedout);
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

        if (signal_run(process_current))
        {
            errno = -EINTR;
            goto free_data;
        }

        if (!process_is_running(process_current))
        {
            // FIXME: I don't have any other idea for the moment; this makes sure that process won't
            // go to sleep (thus avoiding exit)
            errno = -ERESTART;
            goto free_data;
        }

        if (timedout)
        {
            delete_array(data, nfds);
            return 0;
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

free_data:
    if (data)
    {
        delete_array(data, nfds);
    }
    return errno;
}

int sys_poll(struct pollfd* fds, unsigned long nfds, int timeout)
{
    int errno;
    timeval_t t;

    if ((errno = current_vm_verify_array(VERIFY_WRITE, fds, nfds)))
    {
        return errno;
    }

    if (timeout > 0)
    {
        t.tv_sec = timeout;
        t.tv_usec = 0;
    }

    return do_poll(fds, nfds, timeout > 0 ? &t : NULL);
}

struct pselect_params
{
    int nfds;
    fd_set* readfds;
    fd_set* writefds;
    fd_set* errorfds;
    const struct timespec* timeout;
    const sigset_t* sigmask;
};

int sys_pselect(struct pselect_params* params)
{
    int errno, res, count = 0;
    struct pollfd* fds;
    timeval_t t;

#if 0
    if (params->writefds || params->errorfds)
    {
        return -EINVAL;
    }
#endif

    if (!params->nfds)
    {
        params->nfds = 1;
    }

    fds = alloc_array(struct pollfd, params->nfds);

    if (unlikely(!fds))
    {
        return -ENOMEM;
    }

    memset(fds, 0, sizeof(*fds) * params->nfds);

    if (params->readfds)
    {
        for (int i = 0; i < params->nfds; ++i)
        {
            if (bitset_test((uint32_t*)params->readfds->fd_bits, i))
            {
                fds[count].events = POLLIN;
                fds[count].revents = 0;
                fds[count].fd = i;
                count++;
            }
        }
    }

    if (params->timeout)
    {
        t.tv_sec = params->timeout->tv_sec;
        t.tv_usec = params->timeout->tv_nsec / 1000;
    }

    res = do_poll(fds, count, params->timeout ? &t : NULL);

    if (unlikely(errno = errno_get(res)))
    {
        return errno;
    }

    if (params->readfds)
    {
        params->readfds->fd_bits[0] = 0;
    }

    for (int i = 0; i < count; ++i)
    {
        if (fds[i].revents == POLLIN)
        {
            bitset_set((uint32_t*)params->readfds->fd_bits, i);
        }
    }

    delete_array(fds, params->nfds);

    return res;
}

int sys_select(int nfds, fd_set_t* readfds, fd_set_t* writefds, fd_set_t* exceptfds, timeval_t* timeout)
{
    struct pselect_params params = {
        .readfds = readfds,
        .writefds = writefds,
        .errorfds = exceptfds,
        .nfds = nfds,
    };
    UNUSED(nfds);
    UNUSED(readfds);
    UNUSED(writefds);
    UNUSED(exceptfds);
    UNUSED(timeout);
    return sys_pselect(&params);
}
