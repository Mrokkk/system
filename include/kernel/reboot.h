#pragma once

#include <arch/reboot.h>
#include <kernel/compiler.h>
#include <kernel/api/reboot.h>

void NORETURN(machine_shutdown(void));
void NORETURN(machine_reboot(void));
void NORETURN(machine_halt(void));

#define SHUTDOWN_IN_PROGRESS 0xfee1dead
extern unsigned shutdown_in_progress;
