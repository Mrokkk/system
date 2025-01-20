#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/malloc.h>
#include <kernel/module.h>
#include <kernel/reboot.h>
#include <kernel/process.h>
#include <kernel/page_mmio.h>

unsigned shutdown_in_progress;

void prepare_to_shutdown()
{
    extern void page_stats_print();

    file_systems_print();
    page_stats_print();
    mmio_print();
    processes_stats_print();
    fmalloc_stats_print();

    modules_shutdown();
}

int sys_reboot(int magic, int magic2, int cmd)
{
    flags_t flags;

    if (magic != REBOOT_MAGIC1 ||
        magic2 != REBOOT_MAGIC2 ||
        cmd > REBOOT_CMD_POWER_OFF)
    {
        return -EINVAL;
    }

    irq_save(flags);

    shutdown_in_progress = SHUTDOWN_IN_PROGRESS;
    log_info("magic=%x, magic2=%x, cmd=%u", magic, magic2, cmd);

    prepare_to_shutdown();

    log_info("goodbye!");

    switch (cmd)
    {
        case REBOOT_CMD_POWER_OFF:
            machine_shutdown();
            break;
        case REBOOT_CMD_RESTART:
            machine_reboot();
            break;
    }

    irq_restore(flags);

    return -EBUSY;
}
