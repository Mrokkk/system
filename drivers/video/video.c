#define log_fmt(fmt) "video: " fmt
#include <arch/earlycon.h>
#include <kernel/kernel.h>
#include <kernel/framebuffer.h>

#include "video_driver.h"

READONLY static LIST_DECLARE(drivers);

UNMAP_AFTER_INIT void video_init(void)
{
    video_driver_t* driver;

    list_for_each_entry(driver, &drivers, list_entry)
    {
        if (!driver->initialize())
        {
            log_notice("initialized %s: %ux%ux%u",
                driver->name,
                framebuffer.width,
                framebuffer.height,
                framebuffer.bpp);
            break;
        }
    }

    earlycon_disable();
}

static void video_driver_add(video_driver_t* new)
{
    video_driver_t* tmp = NULL;

    if (list_empty(&drivers))
    {
        list_add(&new->list_entry, &drivers);
        return;
    }

    list_for_each_entry(tmp, &drivers, list_entry)
    {
        if (new->score > tmp->score)
        {
            list_merge(&new->list_entry, &tmp->list_entry);
            return;
        }
    }

    list_add_tail(&new->list_entry, &drivers);
}

UNMAP_AFTER_INIT int video_driver_register(video_driver_t* driver)
{
    list_init(&driver->list_entry);
    video_driver_add(driver);
    return 0;
}
