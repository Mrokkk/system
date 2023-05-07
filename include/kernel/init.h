#pragma once

void arch_setup(void);
void arch_late_setup(void);
int paging_init(void);
int temp_shell(void);

#define INIT_IN_PROGRESS 0xfacade
extern unsigned init_in_progress;
extern char cmdline[];
