#include <kernel/process.h>

int sys_exec(struct pt_regs regs) {

    return arch_exec(&regs);

}
