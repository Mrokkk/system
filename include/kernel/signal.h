#ifndef INCLUDE_KERNEL_SIGNAL_H_
#define INCLUDE_KERNEL_SIGNAL_H_

#include <kernel/kernel.h>

typedef int (*sighandler_t)();

struct sigaction {
    sighandler_t sighandler;
    unsigned int sigmask;
    unsigned int sigflags;
    void (*sigrestorer)(void);
};

#endif /* INCLUDE_KERNEL_SIGNAL_H_ */
