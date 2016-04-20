#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/string.h>

/*
 * Whole modules implementation is temporary
 */

struct kernel_module *modules_list = 0;

/*===========================================================================*
 *                               modules_init                                *
 *===========================================================================*/
KERNEL_INIT(modules_init, INIT_PRIORITY_MED)() {

    struct kernel_module *module = 0;
    unsigned int *temp, i;

    printk("Configuring modules... \n");

    for (i = 0, temp = (unsigned int *)_smodules_data;
            temp <= (unsigned int *)_emodules_data - 3;
            temp = (unsigned int *)((unsigned int)temp + 1)) {

        if (*temp == KERNEL_MODULE_MAGIC) {

            module = (struct kernel_module *)(++temp);

            module_add(module);

            printk("  configuring %s... ", module->name);

            if (module->init != 0) {
               if (module->init())
                   printk("FAILED\n");
               else printk("OK\n");
            }
            i++;

        }
    }

    return 0;

}

/*===========================================================================*
 *                              modules_list_get                             *
 *===========================================================================*/
int modules_list_get(char *buffer) {

    int i, len = 0;
    struct kernel_module *temp;

    len = sprintf(buffer, "Installed modules: \n");
    for (i = 0, temp = modules_list; temp != NULL; i++, temp = temp->next)
        len += sprintf(buffer + len, "module %d : %s\n", i, temp->name);

    return len;

}

/*===========================================================================*
 *                                module_add                                 *
 *===========================================================================*/
int module_add(struct kernel_module *module) {

    struct kernel_module *temp = modules_list;
    struct kernel_module *new = kmalloc(sizeof(struct kernel_module));
    if (!new)
        return ENOMEM;

    memcpy(new, module, sizeof(void *) * 4);
    new->next = 0;
    if (temp == 0) {
        modules_list = new;
        return 0;
    }

    for (; temp->next != NULL; temp = temp->next)
        ;

    temp->next = new;

    kernel_trace("added kernel module \"%s\"", temp->name);

    return 0;

}

/*===========================================================================*
 *                               module_remove                               *
 *===========================================================================*/
int module_remove(struct kernel_module *module) {

    struct kernel_module *temp = modules_list;
    struct kernel_module *prev = 0;

    do {
        if (temp == module) {
            if (prev != NULL)
                prev->next = temp->next;
            kfree(temp);
            if (temp == modules_list)
                modules_list = 0;
        }
        prev = temp;
        temp = temp->next;
    } while (temp != NULL);

    return 0;

}

/*===========================================================================*
 *                                module_find                                *
 *===========================================================================*/
struct kernel_module *module_find(unsigned int this_module) {

    struct kernel_module *temp = modules_list;

    do {
        if (temp->this_module == this_module) {
            return temp;
        }
        temp = temp->next;
    } while (temp != NULL);

    return 0;

}

/*===========================================================================*
 *                              modules_shutdown                             *
 *===========================================================================*/
void modules_shutdown() {

    struct kernel_module *mod;

    for (mod = modules_list; mod; mod = mod->next)
        if (mod->deinit) mod->deinit();

}
