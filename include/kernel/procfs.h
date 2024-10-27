#pragma once

#include <stddef.h>
#include <kernel/process.h>
#include <kernel/seq_file.h>

struct procfs_inode;
typedef struct procfs_inode procfs_inode_t;

#define PID_TO_INO(pid) \
    ((pid) << 8)

#define INO_TO_PID(ino) \
    (((ino) >> 8) & 0xffff)

#define FD_TO_INO(fd) \
    ((fd) << 24)

#define INO_TO_FD(ino) \
    (((ino) >> 24) & (PROCESS_FILES - 1))

#define SELF_INO (1 << 31)

static inline process_t* procfs_process_from_inode(inode_t* inode)
{
    process_t* p = NULL;
    int ino = inode->ino;

    if ((ino & SELF_INO) == SELF_INO)
    {
        p = process_current;
    }
    else if (unlikely(process_find(INO_TO_PID(ino), &p)))
    {
        return NULL;
    }

    return p;
}

static inline process_t* procfs_process_from_seqfile(seq_file_t* s)
{
    return procfs_process_from_inode(s->file->dentry->inode);
}

int procfs_init(void);
void procfs_cleanup(process_t* p);
