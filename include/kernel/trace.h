#pragma once

#define DEBUG_VM            0
#define DEBUG_VM_APPLY      0
#define DEBUG_VM_COPY       0
#define DEBUG_VM_COW        0

#define DEBUG_WAIT_QUEUE    0
#define DEBUG_PAGE          0
#define DEBUG_PAGE_DETAILED 1 // Collects page_alloc caller address
#define DEBUG_KMALLOC       0
#define DEBUG_FMALLOC       0
#define DEBUG_PROCESS       0
#define DEBUG_SIGNAL        0
#define DEBUG_EXCEPTION     0

#define DEBUG_OPEN          0
#define DEBUG_DENTRY        0
#define DEBUG_LOOKUP        0
#define DEBUG_READDIR       0

#define DEBUG_MBFS          0
#define DEBUG_DEVFS         0
#define DEBUG_RAMFS         0

#define DEBUG_SERIAL        0
#define DEBUG_CONSOLE       0
#define DEBUG_CON_SCROLL    1
#define DEBUG_KEYBOARD      0

#define DEBUG_ELF           0
#define DEBUG_MULTIBOOT     0

#define crash() { int* i = (int*)0; *i = 2137; }
