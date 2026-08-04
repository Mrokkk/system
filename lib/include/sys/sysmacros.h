#pragma once

#include <sys/cdefs.h>
#include <kernel/api/dev.h>

__BEGIN_DECLS

#define minor(dev)          _MINOR(dev)
#define major(dev)          _MAJOR(dev)
#define mkdev(maj, min)     _MKDEV(maj, min)

__END_DECLS
