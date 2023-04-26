#include <kernel/page.h>
#include <kernel/memory.h>

#include <arch/multiboot.h>

char* bootloader_name;
uint32_t module_start;
uint32_t module_end;

void* disk_img;
void* tux;
uint32_t tux_size;

static modules_table_t modules_table;
struct framebuffer* framebuffer_ptr;

modules_table_t* modules_table_get(void)
{
    return &modules_table;
}

static inline void multiboot_mmap_read(struct multiboot_info* mb)
{
    struct memory_map* mm;
    int mm_type;
    int i;

    for (mm = ptr(mb->mmap_addr), i = 0;
         mm->base_addr_low + (mm->length_low - 1) != 0xffffffff;
         ++i)
    {
        mm = ptr((uint32_t)mm + mm->size + 4);

        switch (mm->type)
        {
            case MULTIBOOT_MEMORY_AVAILABLE: mm_type = MMAP_TYPE_AVL; break;
            case MULTIBOOT_MEMORY_RESERVED: mm_type = MMAP_TYPE_RES; break;
            case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE: mm_type = MMAP_TYPE_DEV; break;
            case MULTIBOOT_MEMORY_NVS: mm_type = MMAP_TYPE_DEV; break;
            default: mm_type = MMAP_TYPE_NDEF;
        }

        memory_area_add(mm->base_addr_low, mm->length_low, mm_type);
    }
}

static inline void multiboot_drives_read(struct multiboot_info* mb)
{
    struct drive* drive = ptr(mb->drives_addr);
    for (; addr(drive) < mb->drives_addr + mb->drives_length;)
    {
        log_info("drive %u; mode %x", drive->drive_number, drive->drive_mode);
        drive = ptr(addr(drive) + drive->size);
    }
}

static inline void multiboot_modules_read(struct multiboot_info* mb)
{
    modules_table.count = mb->mods_count;
    modules_table.modules = virt_ptr(mb->mods_addr);

    struct module* mod = ptr(mb->mods_addr);

    for (size_t i = 0; i < modules_table.count; ++i)
    {
        log_debug(DEBUG_MULTIBOOT, "%d: %s = %x : %x",
            i,
            (char*)virt(mod->string), // FIXME: why there was +1?
            virt(mod->mod_start),
            virt(mod->mod_end));

        if (!strcmp((char*)virt(mod->string), "tux.tga"))
        {
            tux = virt_ptr(mod->mod_start);
            tux_size = mod->mod_end - mod->mod_start;
        }
        if (!strcmp((char*)virt(mod->string), "disk.img"))
        {
            disk_img = virt_ptr(mod->mod_start);
        }

        if (!module_start)
        {
            module_start = virt(mod->mod_start);
        }

        module_end = virt(mod->mod_end);
        mod++;
    }
}

static inline void multiboot_fb_read(struct multiboot_info* mb)
{
    struct framebuffer* framebuffer = &mb->framebuffer;
    framebuffer_ptr = virt_ptr(framebuffer);
}

char* multiboot_read(struct multiboot_info* mb, uint32_t magic)
{
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
        log_warning("not Multiboot v1 compilant bootloader!");
        return 0;
    }

    if (mb->flags & MULTIBOOT_FLAGS_MMAP_BIT)
    {
        multiboot_mmap_read(mb);
    }
    if (mb->flags & MULTIBOOT_FLAGS_DRIVES_BIT)
    {
        multiboot_drives_read(mb);
    }
    if (mb->flags & MULTIBOOT_FLAGS_BOOTDEV_BIT)
    {
        // TODO
    }
    if (mb->flags & MULTIBOOT_FLAGS_BL_NAME_BIT)
    {
        bootloader_name = virt_ptr(mb->bootloader_name);
    }
    if (mb->flags & MULTIBOOT_FLAGS_MODS_BIT)
    {
        multiboot_modules_read(mb);
    }
    if (mb->flags & MULTIBOOT_FLAGS_FB_BIT)
    {
        multiboot_fb_read(mb);
    }

    return virt_ptr(mb->cmdline);
}
