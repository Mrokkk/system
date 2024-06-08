#pragma once

#include <kernel/termios.h>

int tcgetattr(int fd, struct termios* termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios* termios_p);
int tcflow(int fd, int action);
