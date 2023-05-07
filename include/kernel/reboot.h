#pragma once

#include <arch/reboot.h>

#define REBOOT_MAGIC1 ((int)0xabcddcba)
#define REBOOT_MAGIC2 ((int)0x2137)

#define REBOOT_CMD_RESTART 1
#define REBOOT_CMD_POWER_OFF 2

int reboot(int magic, int magic2, int cmd);

void machine_shutdown(void);
void machine_reboot(void);
void machine_halt(void);

#define SHUTDOWN_IN_PROGRESS 0xfee1dead
extern unsigned shutdown_in_progress;
