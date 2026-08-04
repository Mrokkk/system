#pragma once

#include <sys/cdefs.h>
#include <kernel/api/termios.h>

__BEGIN_DECLS

int tcgetattr(int fd, struct termios* termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios* termios_p);

speed_t cfgetispeed(const struct termios* termios_p);
speed_t cfgetospeed(const struct termios* termios_p);

int cfsetispeed(struct termios* termios_p, speed_t speed);
int cfsetospeed(struct termios* termios_p, speed_t speed);

int tcflush(int fd, int queue_selector);
int tcflow(int fd, int action);

__END_DECLS
