#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/string.h>

LIST_DECLARE(modules);

int modules_init()
{
    struct kernel_module* module;

    for (module = (struct kernel_module*)_smodules_data;
         module < (struct kernel_module*)_emodules_data;
         module++)
    {
        module_add(module);

        if (module->init != 0)
        {
           if (module->init())
           {
               log_debug("%s: FAIL", module->name);
           }
           else
           {
               log_debug("%s: SUCCESS", module->name);
           }
        }
    }

    return 0;
}

struct kernel_module* module_find(unsigned int this_module)
{
    struct kernel_module* temp;

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
    struct kernel_module* temp;

    list_for_each_entry(temp, &modules, modules)
    {
        temp->deinit();
    }
}
