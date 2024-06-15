#include <kernel/fs.h>
#include <kernel/elf.h>
#include <kernel/path.h>
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

int module_register(kmod_t* module)
{
    log_info("registered module: %s", module->name);
    module->init();
    return 0;
}

int sys_init_module(const char* name)
{
    int errno;
    scoped_file_t* file = NULL;
    kmod_t* module = NULL;
    kmod_t* temp;

    if ((errno = path_validate(name)))
    {
        return errno;
    }

    if ((errno = do_open(&file, name, O_RDONLY, 0)))
    {
        log_warning("no module found!");
        return -ENOENT;
    }

    if ((errno = elf_module_load(name, file, &module)))
    {
        log_warning("failed to load module: %d", errno);
        return errno;
    }

    list_for_each_entry(temp, &modules, modules)
    {
        if (!strcmp(temp->name, module->name))
        {
            return -EEXIST;
        }
    }

    log_info("%s: initializing", name);

    if ((errno = module->init()))
    {
        log_info("%s: failed with %d", errno);
        return errno;
    }

    list_add_tail(&module->modules, &modules);

    return 0;
}
