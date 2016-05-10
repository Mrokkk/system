#ifndef ARCH_X86_INCLUDE_ARCH_SEMAPHORE_H_
#define ARCH_X86_INCLUDE_ARCH_SEMAPHORE_H_

#include <kernel/process.h>

static inline void semaphore_up(semaphore_t *sem) {

    asm volatile(
        "incl %0;"
        "call __semaphore_wake;"
        : "=m" (sem->count)
        : "c" (sem)
        : "memory");

}

static inline void semaphore_down(semaphore_t *sem) {

    asm volatile(
        "1:"
        "decl %0;"
        "js 2f;"
        "jmp 3f;"
        "2:"
        "call __semaphore_sleep;"
        "3: "
        : "=m" (sem->count)
        : "c" (sem)
        : "memory");

}

#endif /* ARCH_X86_INCLUDE_ARCH_SEMAPHORE_H_ */
