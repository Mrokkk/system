#pragma once

#include <common/bits.h>

__BEGIN_DECLS

#define CLONE_FS            (1 << 0)
#define CLONE_FILES         (1 << 1)
#define CLONE_SIGHAND       (1 << 2)
#define CLONE_VM            (1 << 3)

int clone(int (*fn)(void*), void* stack, int flags, void* arg, void* tls);

__END_DECLS
