#pragma once

struct spinlock
{
    volatile unsigned int lock;
};

typedef struct spinlock spinlock_t;

#define SPINLOCK_UNLOCKED   0
#define SPINLOCK_LOCKED     1

#define SPINLOCK_INIT() \
    { SPINLOCK_UNLOCKED }

#define SPINLOCK_DECLARE(spin) \
    spinlock_t spin = SPINLOCK_INIT()

#define spinlock_init(x)    (x)->lock = SPINLOCK_UNLOCKED

// Include arch-dependent header, which defines
// all spin-lock operations, i.e. lock, unlock,
// trylock
#include <arch/spinlock.h>
