#include <kernel/init.h>
#include <kernel/page.h>
#include <kernel/ksyms.h>
#include <kernel/memory.h>
#include <kernel/sections.h>

#include <arch/multiboot.h>

#define DEBUG_MULTIBOOT 0

void* disk_img;
void* ksyms_start;
void* ksyms_end;

static void multiboot_modules_read(multiboot_info_t* mb)
{
    char* name;
    uintptr_t module_start = 0, module_end = 0;
    multiboot_module_t* mod = ptr(mb->mods_addr);

    for (size_t i = 0; i < mb->mods_count; ++i)
    {
        name = virt_ptr(mod->string);
        log_debug(DEBUG_MULTIBOOT, "%d: %s = %x : %x",
            i,
            name,
            virt(mod->mod_start),
            virt(mod->mod_end));

        if (!strcmp(name, "disk.img"))
        {
            disk_img = virt_ptr(mod->mod_start);
        }
        else if (!strcmp(name, "kernel.map"))
        {
            ksyms_start = virt_ptr(mod->mod_start);
            ksyms_end = virt_ptr(mod->mod_end);
        }

        if (!module_start)
        {
            module_start = virt((uintptr_t)mod->mod_start);
        }

        module_end = virt((uintptr_t)mod->mod_end);
        mod++;
    }

    if (module_start && module_end)
    {
        section_add("modules", ptr(module_start), ptr(module_end), SECTION_READ);
    }
}

UNMAP_AFTER_INIT char* multiboot_read(va_list args)
{
    multiboot_info_t* mb = va_arg(args, void*);
    uintptr_t magic = va_arg(args, uintptr_t);

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
        log_warning("not Multiboot v1 compilant bootloader, magic = %p!", (void*)magic);
        bootloader_name = "unknown";
        return "";
    }

    if (mb->flags & MULTIBOOT_FLAGS_BL_NAME_BIT)
    {
        bootloader_name = virt_ptr(mb->bootloader_name);
    }
    if (mb->flags & MULTIBOOT_FLAGS_MODS_BIT)
    {
        multiboot_modules_read(mb);
    }

    return virt_ptr(mb->cmdline);
}
