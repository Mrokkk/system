#pragma once

int video_init(void);
int display_write(struct file*, char* buffer, int size);
void display_print(const char* text);
