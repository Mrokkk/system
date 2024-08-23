#pragma once

#include <stdint.h>
#include <kernel/list.h>

struct inode;
typedef struct dentry dentry_t;

struct dentry
{
    struct inode* inode;
    char*         name;
    list_head_t   cache;
    list_head_t   child;
    list_head_t   subdirs;
    dentry_t*     parent;
    uint16_t      refcount;
};

dentry_t* dentry_create(struct inode* inode, dentry_t* parent_dentry, const char* name);
dentry_t* dentry_lookup(dentry_t* parent_dentry, const char* name);
void dentry_delete(dentry_t* dentry);
