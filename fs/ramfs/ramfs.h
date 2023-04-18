#pragma once

struct inode;

typedef struct ram_node
{
    char name[256];
    char type;
    void* data;
    struct inode* inode;
} ram_node_t;

typedef struct ram_dirent
{
    ram_node_t* entry;
    struct ram_dirent* next;
} ram_dirent_t;
