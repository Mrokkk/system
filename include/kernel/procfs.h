#pragma once

#include <stddef.h>
#include <kernel/page.h>
#include <kernel/process.h>
#include <kernel/seq_file.h>

struct procfs_inode;
typedef struct procfs_inode procfs_inode_t;

#define PID_TO_INO(pid) \
    ((pid) << 8)

#define INO_TO_PID(ino) \
    ((ino) >> 8)

#define SELF_INO (1 << 31)

static inline process_t* procfs_process_from_seqfile(seq_file_t* s)
{
    process_t* p = NULL;
    int ino = s->file->inode->ino;

    if ((ino & SELF_INO) == SELF_INO)
    {
        p = process_current;
    }
    else if (process_find(INO_TO_PID(ino), &p))
    {
        return NULL;
    }

    return p;
}

int procfs_init(void);
