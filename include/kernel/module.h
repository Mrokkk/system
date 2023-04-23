#pragma once

#include <kernel/list.h>
#include <kernel/compiler.h>

struct kernel_module
{
    int (*init)();
    int (*deinit)();
    char* name;
    unsigned int this_module;
    struct list_head modules;
};

typedef struct kernel_module kmod_t;

#define KERNEL_MODULE(n) \
    static int kmodule_init(); \
    static int kmodule_deinit(); \
    __attribute__ ((section(".modules_data"))) \
    kmod_t km_##n = { \
        .init = kmodule_init, \
        .deinit = kmodule_deinit, \
        .name = #n, \
        .this_module = addr(&km_##n), \
        .modules = LIST_INIT(km_##n.modules) \
    }; \
    static unsigned int this_module = addr(&km_##n); \

#define module_init(init) \
    static int kmodule_init() __alias(init)

#define module_exit(exit) \
    static int kmodule_deinit() __alias(exit)

int modules_init();
void modules_shutdown();

void module_add(kmod_t* new);
void module_remove(kmod_t* module);
kmod_t* module_find(unsigned int this_module);
