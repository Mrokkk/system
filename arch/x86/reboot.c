#include <arch/io.h>
#include <arch/irq.h>

#include <kernel/clock.h>
#include <kernel/kernel.h>
#include <kernel/reboot.h>

extern void real_mode_reboot(void);

void (*reboot_fn)(void);
void (*shutdown_fn)(void);

static void prepare(void)
{
    cli();
    panic_mode_enter();
    irq_chip_disable();
    clock_sources_shutdown();
}

void NORETURN(machine_shutdown(void))
{
    prepare();

    log_notice("performing shutdown...");

    shutdown_fn();

    io_delay(500000);
    log_warning("shutdown: timeout");

    machine_halt();
}

void NORETURN(machine_reboot(void))
{
    prepare();

    log_notice("performing reboot...");

    if (reboot_fn)
    {
        reboot_fn();
        io_delay(500000);
        log_warning("reboot: timeout; performing via real mode");
    }

    real_mode_reboot();

    machine_halt();
}

void NORETURN(machine_halt(void))
{
    cli();
    for (;; halt());
}
