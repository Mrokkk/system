#pragma once

#include <kernel/list.h>
#include <kernel/time.h>
#include <kernel/process.h>

typedef struct ktimer ktimer_t;
typedef void (*timer_cb_t)(ktimer_t* timer);

enum
{
    KTIMER_TYPE_MASK     = (1 << 31),
    KTIMER_REPEATING     = (0 << 31),
    KTIMER_ONESHOT       = (1 << 31),
    KTIMER_AUTODELETE    = TIMER_AUTO_RELEASE,
    KTIMER_PRESERVE_EXEC = TIMER_PRESERVE_EXEC,
};

struct ktimer
{
    timeval_t       deadline;
    process_t*      process;
    timeval_t       interval;
    timer_t         id;
    int             flags;
    void*           data;
    timer_cb_t      cb;
    timer_cb_t      cleanup;
    list_head_t     list_entry;
    list_head_t     process_timers;
};

timer_t ktimer_create_and_start(
    int flags,
    timeval_t timerval,
    timer_cb_t cb,
    void* data);

int ktimer_delete(ktimer_t* timer);
void process_ktimers_exit(process_t* p);
