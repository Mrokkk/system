#pragma once

#include <kernel/api/dev.h>

#define minor(dev)          _MINOR(dev)
#define major(dev)          _MAJOR(dev)
#define mkdev(maj, min)     _MKDEV(maj, min)
