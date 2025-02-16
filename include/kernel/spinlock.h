#pragma once

#include <kernel/kernel.h>
#include <kernel/compiler.h>

struct spinlock
{
    volatile unsigned int lock;
};

typedef struct spinlock spinlock_t;

struct spinlock_irq_lock
{
    spinlock_t* spinlock;
    flags_t     flags;
};

typedef struct spinlock_irq_lock spinlock_irq_lock_t;

#define SPINLOCK_UNLOCKED   0
#define SPINLOCK_LOCKED     1

#define SPINLOCK_INIT() \
    { SPINLOCK_UNLOCKED }

#define SPINLOCK_DECLARE(spin) \
    spinlock_t spin = SPINLOCK_INIT()

#define spinlock_init(x)    ({ (x)->lock = SPINLOCK_UNLOCKED; })

#define scoped_spinlock_lock(spin) \
    CLEANUP(__spinlock_unlock) spinlock_t* __s = ({ spinlock_lock(spin); (spin); })

#define scoped_spinlock_irq_lock(spin) \
    CLEANUP(__spinlock_irq_unlock) spinlock_irq_lock_t __s = ({ \
        flags_t __flags; \
        irq_save(__flags); \
        spinlock_lock(spin); \
        (spinlock_irq_lock_t){ \
            .spinlock = (spin), \
            .flags = __flags \
        }; \
    })

// Include arch-dependent header, which defines
// all spin-lock operations, i.e. lock, unlock,
// trylock
#include <arch/spinlock.h>

static inline void __spinlock_unlock(spinlock_t** s)
{
    spinlock_unlock(*s);
}

static inline void __spinlock_irq_unlock(spinlock_irq_lock_t* s)
{
    spinlock_unlock(s->spinlock);
    irq_restore(s->flags);
}
