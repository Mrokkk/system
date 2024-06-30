#pragma once

#include <kernel/vm.h>
#include <kernel/list.h>

struct inode;
struct binary;
typedef struct binary binary_t;

struct binary
{
    int count;
    void* entry;
    struct inode* inode;
    unsigned int brk;
    vm_area_t* stack_vma;
};
