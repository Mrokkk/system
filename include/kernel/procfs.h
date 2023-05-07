#pragma once

#include <stddef.h>
#include <kernel/page.h>
#include <kernel/process.h>

struct procfs_inode;
typedef struct procfs_inode procfs_inode_t;

int procfs_init(void);
