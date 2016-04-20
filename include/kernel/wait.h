#ifndef INCLUDE_KERNEL_WAIT_H_
#define INCLUDE_KERNEL_WAIT_H_

#include <kernel/process.h>

struct wait_queue {
    struct process *process;
    struct wait_queue *next;
};

#endif /* INCLUDE_KERNEL_WAIT_H_ */
