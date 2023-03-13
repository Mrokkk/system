#pragma once

typedef unsigned short magic_t;

#define USE_MAGIC

#define DENTRY_MAGIC    0x2334
#define INODE_MAGIC     0x2335

#ifdef USE_MAGIC
#define MAGIC_NUMBER magic_t magic
#else
#define MAGIC_NUMBER
#endif
