#pragma once

#include <kernel/list.h>
#include <kernel/magic.h>

struct inode;

struct dentry
{
    MAGIC_NUMBER;
    struct inode* inode;
    char* name;
    struct list_head cache;
    struct list_head child;
    struct list_head subdirs;
    struct dentry* parent;
};

typedef struct dentry dentry_t;

void dentry_init(dentry_t* dentry);
dentry_t* dentry_create(struct inode* inode, dentry_t* parent_dentry, const char* name);
dentry_t* dentry_lookup(dentry_t* parent_dentry, const char* name);
char* dentry_print(const void* data, char* str);
