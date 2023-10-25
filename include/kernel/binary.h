#pragma once

#include <kernel/vm.h>
#include <kernel/list.h>

struct inode;
struct binary;
struct so_segment;
typedef struct binary binary_t;
typedef struct so_segment so_segment_t;

struct so_segment
{
    uint32_t start, end;
    page_t* pages;
    int vm_flags;
    so_segment_t* next;
};

struct library
{
    int count;
    struct inode* inode;
    so_segment_t* segments;
    list_head_t list;
};

struct binary
{
    int count;
    void* entry;
    struct inode* inode;
    page_t* symbols_pages;
    unsigned int brk;
    vm_area_t* stack_vma;
};
