#pragma once

#include <kernel/api/types.h>
#include <kernel/page_types.h>

struct inode;

typedef struct ram_sb ram_sb_t;
typedef struct ram_node ram_node_t;
typedef struct ram_dirent ram_dirent_t;

// Len selected to have ram_node_t with size 64 B
#define RAMFS_NAME_MAX_LEN 44

struct ram_sb
{
    ino_t last_ino;
    ram_node_t* root;
};

struct ram_node
{
    char name[RAMFS_NAME_MAX_LEN];
    mode_t mode;
    struct inode* inode;
    page_t* pages;
    size_t size;
    size_t max_capacity;
};
