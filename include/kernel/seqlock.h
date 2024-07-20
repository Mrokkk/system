#pragma once

#include <kernel/kernel.h>
#include <kernel/spinlock.h>

typedef struct seqlock seqlock_t;

struct seqlock
{
    spinlock_t lock;
    unsigned   seq;
};

#define SEQLOCK_INIT() \
    { SPINLOCK_INIT(), 0 }

static inline void seqlock_init(seqlock_t* s)
{
    spinlock_init(&s->lock);
    s->seq = 0;
}

static inline void seqlock_write_lock(seqlock_t* s)
{
    spinlock_lock(&s->lock);
    ++s->seq;
    mb();
}

static inline void seqlock_write_unlock(seqlock_t* s)
{
    mb();
    ++s->seq;
    spinlock_unlock(&s->lock);
}

static inline unsigned seqlock_read_begin(seqlock_t* s)
{
    unsigned ret;

repeat:
    // Wait for the transaction to finish
    ret = ACCESS_ONCE(s->seq);

    if (unlikely(ret & 1))
    {
        cpu_relax();
        goto repeat;
    }

    mb();

    return ret;
}

static inline int seqlock_read_check(seqlock_t* s, unsigned start)
{
    mb();
    return unlikely(s->seq != start);
}
