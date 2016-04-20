#ifndef __MODULE_H_
#define __MODULE_H_

#include <kernel/compiler.h>

struct kernel_module {
    int (*init)();
    int (*deinit)();
    char *name;
    unsigned int this_module;
    struct kernel_module *next;
};

#define KERNEL_MODULE_MAGIC 0x254E0401

#define KERNEL_MODULE(name) \
    static int kmodule_init();              \
    static int kmodule_deinit();            \
    static unsigned int this_module;        \
    __attribute__ ((section(".modules_data"))) \
    unsigned int km_##name[] = {            \
            KERNEL_MODULE_MAGIC,            \
            (unsigned int)kmodule_init,     \
            (unsigned int)kmodule_deinit,   \
            (unsigned int)#name,            \
            (unsigned int)&this_module      \
    }

#ifndef __alias
#define __alias(x)
#endif

#define module_init(init) \
    static int kmodule_init() __alias(init)

#define module_exit(exit) \
    static int kmodule_deinit() __alias(exit)

int modules_init();
int modules_list_get();
int module_add(struct kernel_module *module);
int module_remove(struct kernel_module *module);
void modules_shutdown();
struct kernel_module *module_find(unsigned int this_module);

#endif
