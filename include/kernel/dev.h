#pragma once

typedef unsigned short dev_t;
#define MINOR(dev) ((dev) & 0xff)
#define MAJOR(dev) ((dev) >> 8)
#define MKDEV(maj, min) (((maj) << 8) | (min))
#define NODEV    0xffff
