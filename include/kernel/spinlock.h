#ifndef INCLUDE_KERNEL_SPINLOCK_H_
#define INCLUDE_KERNEL_SPINLOCK_H_

/* I hate typedefs because of hiding real things,
 * but that's the only case they are justified
 */
typedef struct spinlock {
    volatile unsigned int lock;
} spinlock_t;

#define SPINLOCK_UNLOCKED   0
#define SPINLOCK_LOCKED     1

#define SPINLOCK_INIT() \
    { SPINLOCK_UNLOCKED }

#define SPINLOCK_DECLARE(spin) \
    spinlock_t spin = SPINLOCK_INIT()

#define spinlock_init(x)    (x)->lock = SPINLOCK_UNLOCKED

/* Include arch-dependent header, which defines
 * all spin-lock operations, i.e. lock, unlock,
 * trylock
 */
#include <arch/spinlock.h>

#endif /* INCLUDE_KERNEL_SPINLOCK_H_ */
