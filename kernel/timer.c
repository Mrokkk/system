#include <kernel/time.h>
#include <kernel/mutex.h>
#include <kernel/timer.h>
#include <kernel/signal.h>
#include <kernel/process.h>

#define DEBUG_TIMER 0

extern timeval_t timestamp;

static ktimer_list_t timer_list = {
    .lock = MUTEX_INIT(timer_list.lock),
    .timers = LIST_INIT(timer_list.timers)
};

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
    if (timer->type == KTIMER_REPEATING)
    {
        memcpy(&timer->deadline, &timestamp, sizeof(timer->deadline));
        ts_add(&timer->deadline, &timer->duration);
        ktimer_add(timer);
    }
    else
    {
        list_del(&timer->process_timers);
        delete(timer);
    }
}

void ktimer_run(void)
{
    if (!mutex_try_lock(&timer_list.lock))
    {
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
}

int ktimer_create(int type, timeval_t timerval, timer_cb_t cb, void* data)
{
    static int last_id;
    ktimer_t* timer = alloc(ktimer_t);

    if (unlikely(!timer))
    {
        return -ENOMEM;
    }

    int id = ++last_id;
    timer->cb = cb;
    timer->id = id;
    timer->data = data;
    timer->type = type;
    timer->process = process_current;

    list_init(&timer->list_entry);
    list_init(&timer->process_timers);

    list_add_tail(&timer->process_timers, &process_current->timers);

    memcpy(&timer->duration, &timerval, sizeof(timerval));
    ts_add(&timerval, &timestamp);
    memcpy(&timer->deadline, &timerval, sizeof(timerval));

    mutex_lock(&timer_list.lock);
    ktimer_add(timer);
    mutex_unlock(&timer_list.lock);

    return id;
}

int ktimer_delete(ktimer_t* timer)
{
    mutex_lock(&timer_list.lock);
    list_del(&timer->list_entry);
    list_del(&timer->process_timers);
    mutex_unlock(&timer_list.lock);
    delete(timer);
    return 0;
}

void ktimers_stop(process_t* p)
{
    ktimer_t* temp;
    ktimer_t* temp2;

    mutex_lock(&timer_list.lock);
    list_for_each_entry(temp, &p->timers, process_timers)
    {
        list_del(&temp->list_entry);
    }
    mutex_unlock(&timer_list.lock);

    for (temp = list_next_entry(&p->timers, ktimer_t, process_timers); &temp->process_timers != &p->timers;)
    {
        temp2 = list_next_entry(&temp->process_timers, ktimer_t, process_timers);
        delete(temp);
        temp = temp2;
    }
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
    ktimer_t* temp;

    list_for_each_entry(temp, &process_current->timers, process_timers)
    {
        if (temp->id == process_current->alarm)
        {
            left = temp->deadline.tv_sec - timestamp.tv_sec;
            ktimer_delete(temp);
            if (seconds == 0)
            {
                return left;
            }
            break;
        }
    }

    id = ktimer_create(KTIMER_ONESHOT, (timeval_t){.tv_sec = seconds, .tv_usec = 0}, alarm_callback, NULL);

    if (unlikely(errno = errno_get(id)))
    {
        return errno;
    }

    return left;
}
