#pragma once

#include <kernel/list.h>
#include <kernel/time.h>
#include <kernel/process.h>

typedef struct ktimer ktimer_t;
typedef struct ktimer_list ktimer_list_t;
typedef void (*timer_cb_t)(ktimer_t* timer);

enum
{
    KTIMER_REPEATING,
    KTIMER_ONESHOT,
};

struct ktimer
{
    timeval_t       deadline;
    process_t*      process;
    timeval_t       duration;
    int             id;
    int             type;
    void*           data;
    timer_cb_t      cb;
    list_head_t     list_entry;
    list_head_t     process_timers;
};

struct ktimer_list
{
    mutex_t lock;
    list_head_t timers;
};

int ktimer_create(
    int type,
    timeval_t timerval,
    timer_cb_t cb,
    void* data);

int ktimer_delete(ktimer_t* timer);
void ktimer_run(void);
void ktimers_stop(process_t* p);
