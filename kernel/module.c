#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/string.h>

static LIST_DECLARE(modules);

UNMAP_AFTER_INIT int modules_init()
{
    kmod_t* module;

    for (module = (kmod_t*)_smodules_data; module < (kmod_t*)_emodules_data; module++)
    {
        module_add(module);

        if (!module->init)
        {
            continue;
        }

        if (module->init())
        {
            log_error("%s: FAIL", module->name);
        }
    }

    return 0;
}

UNMAP_AFTER_INIT void module_add(kmod_t* new)
{
    list_add_tail(&new->modules, &modules);
}

void module_remove(kmod_t* module)
{
    list_del(&module->modules);
}

kmod_t* module_find(unsigned int this_module)
{
    kmod_t* temp;

    list_for_each_entry(temp, &modules, modules)
    {
        if (temp->this_module == this_module)
        {
            return temp;
        }
    }

    return NULL;
}

void modules_shutdown()
{
    kmod_t* temp;

    list_for_each_entry(temp, &modules, modules)
    {
        temp->deinit();
    }
}
