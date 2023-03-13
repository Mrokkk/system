#pragma once

int keyboard_init(void);
int keyboard_read(struct file* file, char* buffer, int size);
