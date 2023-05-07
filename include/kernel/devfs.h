#pragma once

#include <kernel/dev.h>

struct file_operations;

int devfs_init(void);
int devfs_register(const char* name, dev_t major, dev_t minor);
int devfs_blk_register(const char* name, dev_t major, dev_t minor, struct file_operations* ops);
struct file_operations* devfs_blk_get(const char* name, dev_t* dev);
