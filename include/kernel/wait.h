#ifndef INCLUDE_KERNEL_WAIT_H_
#define INCLUDE_KERNEL_WAIT_H_

#include <kernel/list.h>

struct process;

/*
 * Implementation of a wait queue is built on the top
 * of list_head.
 */

struct wait_queue {
    struct process *data;
    struct list_head processes;
};

#define DECLARE_WAIT_QUEUE(name, process) \
    struct wait_queue name = { (void *)process, LIST_INIT(name.processes) }

#define DECLARE_WAIT_QUEUE_HEAD(name) \
        DECLARE_WAIT_QUEUE(name, 0)

static inline void queue_push(struct wait_queue *head, struct wait_queue *new) {

    list_add_tail(&head->processes, &new->processes);

}

static inline int queue_empty(struct wait_queue *head) {

    return list_empty(&head->processes);

}

static inline void queue_del(struct wait_queue *queue) {

    list_del(&queue->processes);

}

static inline struct process *queue_pop(struct wait_queue *head) {

    void *data;
    struct wait_queue *queue;

    queue = list_entry((head)->processes.next, struct wait_queue, processes);
    data = queue->data;

    queue_del(queue);

    return data;

}

#endif /* INCLUDE_KERNEL_WAIT_H_ */
