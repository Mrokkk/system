#ifndef __MODULE_H_
#define __MODULE_H_

#include <kernel/compiler.h>
#include <kernel/list.h>

struct kernel_module {
    int (*init)();
    int (*deinit)();
    char *name;
    unsigned int this_module;
    struct list_head modules;
};

#define KERNEL_MODULE(name) \
    static int kmodule_init();                  \
    static int kmodule_deinit();                \
    static unsigned int this_module;            \
    __attribute__ ((section(".modules_data")))  \
    struct kernel_module km_##name = {          \
            kmodule_init,                       \
            kmodule_deinit,                     \
            #name,                              \
            (unsigned int)&this_module,         \
            LIST_INIT(km_##name.modules)        \
    }

#define module_init(init) \
    static int kmodule_init() __alias(init)

#define module_exit(exit) \
    static int kmodule_deinit() __alias(exit)

extern struct list_head modules;

/*===========================================================================*
 *                                module_add                                 *
 *===========================================================================*/
static inline void module_add(struct kernel_module *new) {

    list_add_tail(&new->modules, &modules);

}

/*===========================================================================*
 *                               module_remove                               *
 *===========================================================================*/
static inline void module_remove(struct kernel_module *module) {

    list_del(&module->modules);

}

int modules_init();
int modules_list_get();
void modules_shutdown();
struct kernel_module *module_find(unsigned int this_module);

#endif
