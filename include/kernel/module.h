#pragma once

#include <kernel/list.h>
#include <kernel/compiler.h>

typedef int (*kmodule_init_t)();
typedef int (*kmodule_deinit_t)();

struct kernel_module
{
    kmodule_init_t   init;
    kmodule_deinit_t deinit;
    const char*      name;
    unsigned int     this_module;
    list_head_t      modules;
    void*            data;
};

typedef struct kernel_module kmod_t;

#define KERNEL_MODULE(n) \
    static int kmodule_init(); \
    static int kmodule_deinit(); \
    SECTION(.modules_data) \
    kmod_t km_##n = { \
        .init = kmodule_init, \
        .deinit = kmodule_deinit, \
        .name = #n, \
        .this_module = addr(&km_##n), \
        .modules = LIST_INIT(km_##n.modules), \
        .data = NULL, \
    }; \
    static unsigned int MAYBE_UNUSED(this_module) = addr(&km_##n)

#define module_init(init) \
    static int kmodule_init() ALIAS(init)

#define module_exit(exit) \
    static int kmodule_deinit() ALIAS(exit)

int modules_init();
void modules_shutdown();

void module_add(kmod_t* new);
void module_remove(kmod_t* module);
kmod_t* module_find(unsigned int this_module);

int module_register(kmod_t* module);
