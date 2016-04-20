#ifndef __REAL_MODE_H_
#define __REAL_MODE_H_

#include <arch/processor.h>
#include <arch/register.h>

struct real_mode_regs_struct *execute_in_real_mode(unsigned int function_address, struct regs_struct *param);

#endif /* __REAL_MODE_H_ */
