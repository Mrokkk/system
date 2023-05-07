#include <stdio.h>
#include <kernel/reboot.h>

int main()
{
    reboot(REBOOT_MAGIC1, REBOOT_MAGIC2, REBOOT_CMD_POWER_OFF);
    return 0;
}
