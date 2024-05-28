#pragma once

#include <kernel/types.h>

struct inode;
struct ram_sb;
struct ram_node;
struct ram_dirent;

typedef struct ram_sb ram_sb_t;
typedef struct ram_node ram_node_t;
typedef struct ram_dirent ram_dirent_t;

#define RAMFS_NAME_MAX_LEN 32

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
    void* data;
};

struct ram_dirent
{
    ram_node_t* entry;
    ram_dirent_t* next;
};
