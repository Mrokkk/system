#ifndef INCLUDE_KERNEL_WAIT_H_
#define INCLUDE_KERNEL_WAIT_H_

#include <kernel/list.h>
#include <kernel/spinlock.h>

struct process;

/*
 * Implementation of a wait queue is built on the top
 * of list_head.
 */

struct wait_queue {
    struct process *data;
    struct list_head processes;
};

struct wait_queue_head {
    spinlock_t lock;
    struct list_head queue;
};

/*
 * Defines for wait_queue_head
 */
#define WAIT_QUEUE_HEAD_INIT(name) \
    { SPINLOCK_INIT(), LIST_INIT((name).queue) }

#define WAIT_QUEUE_HEAD_DECLARE(name) \
    struct wait_queue_head name = WAIT_QUEUE_HEAD_INIT(name)

/*
 * Defines for wait_queue
 */
#define WAIT_QUEUE_INIT(name, process) \
    { process, LIST_INIT((name).processes) }

#define WAIT_QUEUE_DECLARE(name, process) \
    struct wait_queue name = WAIT_QUEUE_INIT(name, process)


static inline void wait_queue_push(struct wait_queue *new, struct wait_queue_head *head) {

    list_add_tail(&new->processes, &head->queue);

}

static inline int wait_queue_empty(struct wait_queue_head *head) {

    return list_empty(&head->queue);

}

static inline void wait_queue_del(struct wait_queue *queue) {

    list_del(&queue->processes);

}

static inline struct process *wait_queue_pop(struct wait_queue_head *head) {

    void *data;
    struct wait_queue *queue;

    if (wait_queue_empty(head)) return 0;

    queue = list_next_entry(&head->queue, struct wait_queue, processes);
    data = queue->data;

    wait_queue_del(queue);

    return data;

}

#endif /* INCLUDE_KERNEL_WAIT_H_ */
