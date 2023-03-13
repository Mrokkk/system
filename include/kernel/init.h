#pragma once

void arch_setup(void);
void irqs_configure(void);
int paging_init(void);
int temp_shell(void);

#define INIT_IN_PROGRESS 0xfacade
extern unsigned init_in_progress;
