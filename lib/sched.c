#include <sched.h>
#include <stdint.h>
#include <stdlib.h>

static __attribute__((regparm(1))) void thread_exit(int exit_code)
{
    exit(exit_code);
}

int LIBC(clone)(int (*fn)(void*), void* stack, int flags, void* arg, void* tls);

#define PUSH(val, stack) \
    do { (stack)--; *stack = (typeof(*stack))(val); } while (0)

int clone(int (*fn)(void*), void* stack, int flags, void* arg, void* tls)
{
    uintptr_t* stack_ptr = stack;

    PUSH(arg, stack_ptr);
    PUSH(&thread_exit, stack_ptr);

    return LIBC(clone)(fn, stack_ptr, flags, NULL, tls);
}
