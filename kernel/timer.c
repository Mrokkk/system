#include <kernel/time.h>
#include <kernel/mutex.h>
#include <kernel/timer.h>
#include <kernel/signal.h>
#include <kernel/process.h>

#define DEBUG_TIMER 0

typedef struct ktimer_list ktimer_list_t;

extern timeval_t timestamp;

struct ktimer_list
{
    mutex_t lock;
    list_head_t timers;
};

static ktimer_list_t timer_list = {
    .lock = MUTEX_INIT(timer_list.lock),
    .timers = LIST_INIT(timer_list.timers)
};

#define LOCKED(...) \
    do \
    { \
        scoped_mutex_lock(&timer_list.lock); \
        __VA_ARGS__; \
    } \
    while (0)

static ktimer_t* process_ktimer_find(timer_t id)
{
    ktimer_t* timer;

    list_for_each_entry(timer, &process_current->timers, process_timers)
    {
        if (timer->id == id)
        {
            return timer;
        }
    }

    return NULL;
}

static void ktimer_add(ktimer_t* timer)
{
    ktimer_t* temp;

    if (!list_empty(&timer_list.timers))
    {
        list_head_t* pos;
        list_for_each(pos, &timer_list.timers)
        {
            temp = list_entry(pos, ktimer_t, list_entry);
            if (ts_gt(&temp->deadline, &timer->deadline))
            {
                log_debug(DEBUG_TIMER, "adding %u:%x before %u:%x",
                    timer->id, timer, temp->id, temp);
                list_merge(&timer->list_entry, pos);
                return;
            }
        }
    }

    log_debug(DEBUG_TIMER, "adding %u:%x at the end",
        timer->id, timer, temp->id, temp);

    list_merge(&timer_list.timers, &timer->list_entry);
}

static int timer_overdue(timeval_t* t)
{
    return timestamp.tv_sec > t->tv_sec ||
        (timestamp.tv_sec == t->tv_sec && timestamp.tv_usec >= t->tv_usec);
}

static void ktimer_callback(ktimer_t* timer)
{
    timer->cb(timer);
    if ((timer->flags & KTIMER_TYPE_MASK) == KTIMER_REPEATING)
    {
        ts_add(&timer->deadline, &timer->interval);
        ktimer_add(timer);
    }
    else
    {
        list_del(&timer->process_timers);
        delete(timer);
    }
}

void ktimers_update(void)
{
    if (unlikely(mutex_try_lock(&timer_list.lock)))
    {
        return;
    }

    while (!list_empty(&timer_list.timers))
    {
        ktimer_t* front = list_next_entry(&timer_list.timers, ktimer_t, list_entry);
        if (timer_overdue(&front->deadline))
        {
            log_debug(DEBUG_TIMER, "remove %u:%x", front->id, front);
            list_del(&front->list_entry);
            ktimer_callback(front);
        }
        else
        {
            break;
        }
    }
    mutex_unlock(&timer_list.lock);
}

static int ktimer_delete_internal(ktimer_t* timer)
{
    LOCKED(
        {
            list_del(&timer->list_entry);
            list_del(&timer->process_timers);
        });

    if (timer->cleanup)
    {
        timer->cleanup(timer);
    }

    delete(timer);

    return 0;
}

static ktimer_t* ktimer_create_internal(timer_cb_t cb, timer_cb_t cleanup, void* data)
{
    static int last_id;
    ktimer_t* timer = alloc(ktimer_t);

    if (unlikely(!timer))
    {
        return ptr(-ENOMEM);
    }

    int id = ++last_id;
    timer->cb = cb;
    timer->id = id;
    timer->data = data;
    timer->cleanup = cleanup;
    timer->process = process_current;

    list_init(&timer->list_entry);
    list_init(&timer->process_timers);

    list_add_tail(&timer->process_timers, &process_current->timers);

    return timer;
}

static int ktimer_start_internal(ktimer_t* timer, int flags, timeval_t* value, timeval_t* interval)
{
    timer->flags = flags;
    memcpy(&timer->interval, interval, sizeof(*interval));
    memcpy(&timer->deadline, value, sizeof(*value));
    ts_add(&timer->deadline, &timestamp);

    LOCKED(ktimer_add(timer));

    return 0;
}

timer_t ktimer_create_and_start(int flags, timeval_t value, timer_cb_t cb, void* data)
{
    int errno;
    ktimer_t* timer = ktimer_create_internal(cb, NULL, data);

    if (unlikely(errno = errno_get(timer)))
    {
        return errno;
    }

    if (unlikely(errno = ktimer_start_internal(timer, flags, &value, &value)))
    {
        ktimer_delete_internal(timer);
        return errno;
    }

    return timer->id;
}

int ktimer_delete(timer_t timer_id)
{
    ktimer_t* timer = process_ktimer_find(timer_id);

    if (unlikely(!timer))
    {
        return -EINVAL;
    }

    ktimer_delete_internal(timer);

    return 0;
}

void process_ktimers_exit(process_t* p)
{
    ktimer_t* timer;

    LOCKED(
        list_for_each_entry_safe(timer, &p->timers, process_timers)
        {
            list_del(&timer->list_entry);
            list_del(&timer->process_timers);
            delete(timer);
        });

    ASSERT(list_empty(&p->timers));
}

static void alarm_callback(ktimer_t* timer)
{
    if (unlikely(do_kill(timer->process, SIGALRM)))
    {
        log_warning("%u: cannot send SIGALRM", timer->process->pid);
    }
}

unsigned int sys_alarm(unsigned int seconds)
{
    int id, errno;
    unsigned int left = 0;
    ktimer_t* running;

    if ((running = process_ktimer_find(process_current->alarm)))
    {
        left = running->deadline.tv_sec - timestamp.tv_sec;
        ktimer_delete_internal(running);
        if (seconds == 0)
        {
            return left;
        }
    }

    id = ktimer_create_and_start(KTIMER_ONESHOT, (timeval_t){.tv_sec = seconds, .tv_usec = 0}, alarm_callback, NULL);

    if (unlikely(errno = errno_get(id)))
    {
        return errno;
    }

    return left;
}

static void posix_timer_signal_callback(ktimer_t* timer)
{
    sigevent_t* event = timer->data;
    if (unlikely(do_kill(timer->process, event->sigev_signo)))
    {
        log_warning("%u: cannot send %s", timer->process->pid, signame(event->sigev_signo));
    }
}

static void posix_timer_empty_callback(ktimer_t*)
{
}

static void posix_timer_cleanup(ktimer_t* timer)
{
    sigevent_t* event = timer->data;
    delete(event);
}

int sys_timer_create(clockid_t clockid, sigevent_t* evp, timer_t* timerid)
{
    int errno;
    sigevent_t* event;
    timer_cb_t callback;
    ktimer_t* timer;

    if (unlikely(clockid >= CLOCK_ID_COUNT))
    {
        return -EINVAL;
    }

    if ((errno = current_vm_verify(VERIFY_READ, timerid)) ||
        (evp && (errno = current_vm_verify(VERIFY_READ, evp))))
    {
        return errno;
    }

    if (unlikely(!(event = alloc(typeof(*event)))))
    {
        return -ENOMEM;
    }

    if (evp)
    {
        memcpy(event, evp, sizeof(*evp));
    }
    else
    {
        event->sigev_notify = SIGEV_SIGNAL;
        event->sigev_signo = SIGALRM;
        event->sigev_value.sival_int = 0;
    }

    switch (event->sigev_notify)
    {
        case SIGEV_NONE:
            callback = &posix_timer_empty_callback;
            break;
        case SIGEV_SIGNAL:
            callback = &posix_timer_signal_callback;
            break;
        case SIGEV_THREAD:
            // TODO: add proper support for this
            callback = &posix_timer_signal_callback;
            break;
            // errno = -ENOSYS;
            // goto error;
        default:
            errno = -EINVAL;
            goto error;
    }

    timer = ktimer_create_internal(callback, &posix_timer_cleanup, event);

    if (unlikely(errno = errno_get(timer)))
    {
        goto error;
    }

    *timerid = timer->id;

    return 0;

error:
    delete(event);
    return errno;
}

int sys_timer_delete(timer_t timerid)
{
    ktimer_t* timer = process_ktimer_find(timerid);

    if (unlikely(!timer))
    {
        return 0;
    }

    ktimer_delete_internal(timer);

    return 0;
}

int sys_timer_settime(
    timer_t timerid,
    int flags,
    const struct itimerspec* new_value,
    struct itimerspec* old_value)
{
    int errno;
    ktimer_t* timer = process_ktimer_find(timerid);

    if (unlikely(!timer || timer->process != process_current))
    {
        // FIXME: should be EINVAL
        return 0;
    }

    if (unlikely(errno = current_vm_verify(VERIFY_READ, new_value)) ||
        (old_value && (errno = current_vm_verify(VERIFY_WRITE, old_value))))
    {
        return errno;
    }

    timeval_t val = {
        .tv_sec = new_value->it_value.tv_sec,
        .tv_usec = nsec2usec(new_value->it_value.tv_nsec)
    };

    timeval_t interval = {
        .tv_sec = new_value->it_interval.tv_sec,
        .tv_usec = nsec2usec(new_value->it_interval.tv_nsec)
    };

    flags |= interval.tv_sec == 0 && interval.tv_usec == 0
        ? KTIMER_ONESHOT
        : KTIMER_REPEATING;

    if (old_value)
    {
        timeval_t temp;
        memcpy(&temp, &timer->deadline, sizeof(temp));
        ts_sub(&temp, &timestamp);
        old_value->it_value.tv_sec = temp.tv_sec;
        old_value->it_value.tv_nsec = usec2nsec(temp.tv_usec);
        old_value->it_interval.tv_sec = timer->interval.tv_sec;
        old_value->it_interval.tv_nsec = usec2nsec(timer->interval.tv_usec);
    }

    return ktimer_start_internal(timer, flags, &val, &interval);
}
