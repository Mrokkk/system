#ifndef __APM_H_
#define __APM_H_

#include <kernel/compiler.h>
#include <arch/bios.h>
#include <arch/real_mode.h>
#include <arch/register.h>

#define APM_INTERFACE_REAL_MODE         1
#define APM_INTERFACE_16BIT_PROTECTED   2
#define APM_INTERFACE_32BIT_PROTECTED   3

#define APM_POWER_STATE_STANDBY 1
#define APM_POWER_STATE_SUSPEND 2
#define APM_POWER_STATE_OFF     3

#define APM_DEVICE_ALL          1

#define __APM_CHECK                     0
#define __APM_CONNECT                   1
#define __APM_DISCONNECT                2
#define __APM_POWER_MANAGEMENT_ENABLE   3
#define __APM_POWER_MANAGEMENT_DISABLE  4
#define __APM_POWER_STATE_SET           5

#define APM_ARG0(arg)   ((arg & 0xff) << 4)
#define APM_ARG1(arg)   ((arg & 0xff) << 12)
#define APM_ARG2(arg)   ((arg & 0xff) << 20)

#define APM_CHECK                       (__APM_CHECK)
#define APM_CONNECT(mode)               (__APM_CONNECT | APM_ARG0(mode))
#define APM_DISCONNECT                  (__APM_DISCONNECT)
#define APM_POWER_MANAGEMENT_ENABLE     (__APM_POWER_MANAGEMENT_ENABLE)
#define APM_POWER_MANAGEMENT_DISABLE    (__APM_POWER_MANAGEMENT_DISABLE)
#define APM_POWER_STATE_SET(dev, stat)  (__APM_POWER_STATE_SET | APM_ARG0(stat) | APM_ARG1(dev))

/* apm_function layout:             *
 * 28    20     12      4      0    *
 * | ARG2 | ARG1 | ARG0 | F_NR |    */

extern struct multiboot_apm_table_struct *apm_table;

struct regs_struct *apm_regs_init(unsigned int apm_function, struct regs_struct *regs);
struct regs_struct *apm_make_call(struct regs_struct *regs);
void apm_enable();

/*===========================================================================*
 *                                 apm_call                                  *
 *===========================================================================*/
extern inline int apm_call(struct regs_struct *regs) {

    asm volatile(
        "pushl %%ds;"
        "pushl %%es;"
        "pushl %%fs;"
        "pushl %%gs;"
        "xorl %%edx, %%edx;"
        "mov %%dx, %%ds;"
        "mov %%dx, %%es;"
        "mov %%dx, %%fs;"
        "mov %%dx, %%gs;"
        "pushl %%edi;"
        "pushl %%ebp;"
        "lcall *%%cs:apm_entry;"
        "setc %%al;"
        "popl %%ebp;"
        "popl %%edi;"
        "popl %%gs;"
        "popl %%fs;"
        "popl %%es;"
        "popl %%ds;"
        : "=a" (regs->eax), "=b" (regs->ebx), "=c" (regs->ecx), "=d" (regs->edx)
        : "a" (regs->eax), "b" (regs->ebx), "c" (regs->ecx)
        : "memory", "cc"
    );

    return regs->eax & 0xff;

}

extern inline struct regs_struct *apm_check(struct regs_struct *regs) {

    apm_regs_init(APM_CHECK, regs);
    return apm_make_call(regs);

}

extern inline struct regs_struct *apm_connect(struct regs_struct *regs, unsigned char mode) {

    apm_regs_init(APM_CONNECT(mode), regs);
    return apm_make_call(regs);

}

extern inline struct regs_struct *apm_disconnect(struct regs_struct *regs) {

    apm_regs_init(APM_DISCONNECT, regs);
    apm_make_call(regs);
    apm_table = 0;
    return regs;

}

extern inline struct regs_struct *apm_power_management_enable(struct regs_struct *regs) {

    apm_regs_init(APM_POWER_MANAGEMENT_ENABLE, regs);
    return apm_make_call(regs);

}

extern inline struct regs_struct *apm_power_management_disable(struct regs_struct *regs) {

    apm_regs_init(APM_POWER_MANAGEMENT_DISABLE, regs);
    return apm_make_call(regs);

}

extern inline struct regs_struct *apm_power_state_set(struct regs_struct *regs, unsigned char dev, unsigned char state) {

    apm_regs_init(APM_POWER_STATE_SET(dev, state), regs);
    return apm_make_call(regs);

}

#endif /* __APM_H_ */
