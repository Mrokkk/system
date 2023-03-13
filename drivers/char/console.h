#pragma once

#include <kernel/fs.h>

int console_write(struct file*, char* buffer, int size);
void console_putch(char c);
int console_init(void);
