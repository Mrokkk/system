#pragma once

#include <stddef.h>
#include <kernel/page.h>
#include <kernel/process.h>
#include <kernel/seq_file.h>

struct procfs_inode;
typedef struct procfs_inode procfs_inode_t;

static inline process_t* process_get(seq_file_t* s)
{
    process_t* p = NULL;
    int pid = s->file->inode->ino;

    if (pid == -1)
    {
        p = process_current;
    }
    else if (process_find(pid, &p))
    {
        return NULL;
    }

    return p;
}

int procfs_init(void);
