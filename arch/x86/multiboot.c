#include <kernel/page.h>
#include <kernel/ksyms.h>
#include <kernel/memory.h>
#include <kernel/sections.h>

#include <arch/multiboot.h>

char* bootloader_name;
struct framebuffer* framebuffer_ptr;
void* disk_img;
void* ksyms_start;
void* ksyms_end;

static inline void multiboot_modules_read(multiboot_info_t* mb)
{
    char* name;
    uint32_t module_start = 0, module_end = 0;
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
            module_start = virt(mod->mod_start);
        }

        module_end = virt(mod->mod_end);
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
    uint32_t magic = va_arg(args, uint32_t);

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
        log_warning("not Multiboot v1 compilant bootloader!");
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
