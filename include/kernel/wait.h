#pragma once

#include <kernel/list.h>
#include <kernel/trace.h>
#include <kernel/spinlock.h>

struct process;

struct wait_queue
{
    int flags;
    struct process* data;
    list_head_t processes;
};

typedef struct wait_queue wait_queue_t;

struct wait_queue_head
{
    spinlock_t lock;
    list_head_t queue;
};

typedef struct wait_queue_head wait_queue_head_t;

// Defines for wait_queue_head
#define WAIT_QUEUE_HEAD_INIT(name) \
    { SPINLOCK_INIT(), LIST_INIT((name).queue) }

#define WAIT_QUEUE_HEAD_DECLARE(name) \
    wait_queue_head_t name = WAIT_QUEUE_HEAD_INIT(name)

// Defines for wait_queue
#define WAIT_QUEUE_INIT(name, process) \
    { 0, process, LIST_INIT((name).processes) }

#define WAIT_QUEUE_DECLARE(name, process) \
    wait_queue_t name = WAIT_QUEUE_INIT(name, process)

static inline void wait_queue_head_init(wait_queue_head_t* head)
{
    list_init(&head->queue);
    spinlock_init(&head->lock);
}

static inline void __wait_queue_push(wait_queue_t* new, wait_queue_head_t* head)
{
    spinlock_lock(&head->lock);
    list_add_tail(&new->processes, &head->queue);
    spinlock_unlock(&head->lock);
}

static inline int wait_queue_empty(wait_queue_head_t* head)
{
    return list_empty(&head->queue);
}

static inline void wait_queue_del(wait_queue_t* queue)
{
    list_del(&queue->processes);
}

static inline struct process* __wait_queue_pop(wait_queue_head_t* head)
{
    void* data;
    struct wait_queue* queue;

    if (wait_queue_empty(head))
    {
        return 0;
    }

    spinlock_lock(&head->lock);
    queue = list_next_entry(&head->queue, struct wait_queue, processes);
    data = queue->data;

    wait_queue_del(queue);

    spinlock_unlock(&head->lock);

    return data;
}

static inline struct process* wait_queue_front(wait_queue_head_t* head)
{
    void* data;
    struct wait_queue* queue;

    if (wait_queue_empty(head))
    {
        return 0;
    }

    spinlock_lock(&head->lock);

    queue = list_next_entry(&head->queue, struct wait_queue, processes);
    data = queue->data;

    spinlock_unlock(&head->lock);

    return data;
}

#if DEBUG_WAIT_QUEUE

#include <kernel/printk.h>

#define wait_queue_push(new, head) \
    { log_debug(DEBUG_WAIT_QUEUE, "wait_queue_push: new=%x head=%x", new, head); __wait_queue_push(new, head); }

#define wait_queue_pop(head) \
    ({ log_debug(DEBUG_WAIT_QUEUE, "wait_queue_pop: head=%x", head); __wait_queue_pop(head); })

#else

#define wait_queue_push(new, head) __wait_queue_push(new, head)
#define wait_queue_pop(head) __wait_queue_pop(head)

#endif
