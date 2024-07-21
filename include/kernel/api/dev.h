#pragma once

#include <kernel/api/types.h>

#define _MINOR(dev)         ((dev) & 0xffff)
#define _MAJOR(dev)         ((dev) >> 16)
#define _MKDEV(maj, min)    (((maj) << 16) | (min))
