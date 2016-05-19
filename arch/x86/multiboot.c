#include <kernel/kernel.h>
#include <kernel/mm.h>

#include <arch/multiboot.h>
#include <arch/page.h>

char *bootloader_name;

/*===========================================================================*
 *                           multiboot_mmap_read                             *
 *===========================================================================*/
static inline void multiboot_mmap_read(struct multiboot_info *mb) {

    struct memory_map *mm;
    int i;

    for (mm = (struct memory_map *)mb->mmap_addr, i = 0;
         mm->base_addr_low + (mm->length_low - 1) != 0xffffffff;
         i++)
    {

        mm = (struct memory_map *)((unsigned int)mm + mm->size + 4);
        switch (mm->type) {
            case 1:
                mmap[i].type = MMAP_TYPE_AVL;
                break;
            case 2:
                mmap[i].type = MMAP_TYPE_NA;
                break;
            case 3:
                mmap[i].type = MMAP_TYPE_DEV;
                break;
            case 4:
                mmap[i].type = MMAP_TYPE_DEV;
                break;
            default:
                mmap[i].type = MMAP_TYPE_NDEF;
        }

        mmap[i].base = mm->base_addr_low;
        mmap[i].size = mm->length_low;
    }

    mmap[i].base = 0;

    ram = ram_get();

}

/*===========================================================================*
 *                        multiboot_boot_device_read                         *
 *===========================================================================*/
static inline void multiboot_boot_device_read(struct multiboot_info *mb) {

    (void)mb;

}

/*===========================================================================*
 *                          multiboot_modules_read                           *
 *===========================================================================*/
static inline void multiboot_modules_read(struct multiboot_info *mb) {

    int count = mb->mods_count, i;
    struct module *mod = (struct module *)mb->mods_addr;

    for (i=0; i<count; i++) {
        printk("%d: %s = 0x%x : 0x%x\n", i,
                (char *)virt_address(mod->string) + 1,
                (unsigned int)virt_address(mod->mod_start),
                (unsigned int)virt_address(mod->mod_end)
        );
        if (!strcmp((const char *)virt_address(mod->string) + 1, "symbols")) {
            symbols_read((char *)virt_address(mod->mod_start),
                    mod->mod_end - mod->mod_start);
        }
        mod++;
    }

}

/*===========================================================================*
 *                              multiboot_read                               *
 *===========================================================================*/
int multiboot_read(struct multiboot_info *mb, unsigned int magic) {

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        printk("ERROR: Not Multiboot v1 compilant bootloader!\n");
        return 0;
    }

    mb = (struct multiboot_info *)(virt_address(mb));

    if (mb->flags & MULTIBOOT_FLAGS_MMAP_BIT)
        multiboot_mmap_read(mb);
    if (mb->flags & MULTIBOOT_FLAGS_BOOTDEV_BIT)
        multiboot_boot_device_read(mb);
    if (mb->flags & MULTIBOOT_FLAGS_BL_NAME_BIT)
        bootloader_name = (char *)mb->bootloader_name;
    //if (mb->flags & MULTIBOOT_FLAGS_MODS_BIT)
    //   multiboot_modules_read(mb);

    return virt_address(mb->cmdline);

}
