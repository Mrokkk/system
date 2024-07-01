#pragma once

#include <stddef.h>
#include <kernel/page.h>
#include <kernel/process.h>
#include <kernel/seq_file.h>

struct procfs_inode;
typedef struct procfs_inode procfs_inode_t;

static inline struct process* process_get(seq_file_t* s)
{
    struct process* p = NULL;
    short pid = s->file->inode->ino;

    if (pid == -1)
    {
        p = process_current;
    }
    else
    {
        if (process_find(pid, &p))
        {
            panic("no process with pid %u", pid);
        }
    }

    return p;
}

int procfs_init(void);
