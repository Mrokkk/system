#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/string.h>

/*
 * Whole modules implementation is temporary
 */

LIST_DECLARE(modules);

int modules_init() {

    struct kernel_module *module = 0;

    printk("Configuring modules... \n");

    for (module = (struct kernel_module *)_smodules_data;
         module < (struct kernel_module *)_emodules_data;
         module++) {

        module_add(module);

        printk("  configuring %s... ", module->name);

        if (module->init != 0) {
           if (module->init())
               printk("FAILED\n");
           else printk("OK\n");
        }

    }

    return 0;

}

int modules_list_get(char *buffer) {

    int i = 0, len = 0;
    struct kernel_module *temp;

    len = sprintf(buffer, "Installed modules: \n");
    list_for_each_entry(temp, &modules, modules) {
        len += sprintf(buffer + len, "module %d : %s\n", i, temp->name);
        i++;
    }

    return len;

}

struct kernel_module *module_find(unsigned int this_module) {

    struct kernel_module *temp;

    list_for_each_entry(temp, &modules, modules)
        if (temp->this_module == this_module)
            return temp;

    return 0;

}

void modules_shutdown() {

    struct kernel_module *temp;

    list_for_each_entry(temp, &modules, modules)
        temp->deinit();

}

