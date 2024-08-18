#pragma once

#include <kernel/list.h>

struct inode;

struct dentry
{
    struct inode* inode;
    char* name;
    list_head_t cache;
    list_head_t child;
    list_head_t subdirs;
    struct dentry* parent;
};

typedef struct dentry dentry_t;

void dentry_init(dentry_t* dentry);
dentry_t* dentry_create(struct inode* inode, dentry_t* parent_dentry, const char* name);
dentry_t* dentry_lookup(dentry_t* parent_dentry, const char* name);
dentry_t* dentry_get(struct inode* inode);
