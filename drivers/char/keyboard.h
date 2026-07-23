#pragma once

struct tty;

int keyboard_init(struct tty* tty);
int keyboard_ioctl(struct tty* tty, unsigned long request, void* arg);
