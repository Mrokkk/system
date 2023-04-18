#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/string.h>

LIST_DECLARE(modules);

int modules_init()
{
    kmod_t* module;

    for (module = (kmod_t*)_smodules_data; module < (kmod_t*)_emodules_data; module++)
    {
        module_add(module);

        if (module->init != NULL)
        {
           if (module->init())
           {
               log_info("%s: FAIL", module->name);
           }
           else
           {
               log_info("%s: SUCCESS", module->name);
           }
        }
    }

    return 0;
}

kmod_t* module_find(unsigned int this_module)
{
    kmod_t* temp = NULL;

    list_for_each_entry(temp, &modules, modules)
    {
        if (temp->this_module == this_module)
        {
            return temp;
        }
    }

    return temp;
}

void modules_shutdown()
{
    kmod_t* temp;

    list_for_each_entry(temp, &modules, modules)
    {
        temp->deinit();
    }
}
