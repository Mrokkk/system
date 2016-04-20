#include <kernel/process.h>

int sys_exec(struct pt_regs regs) {

    (void)regs;
    resched();

    return 0;

}
