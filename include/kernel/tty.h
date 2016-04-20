#ifndef __TTY_H_
#define __TTY_H_

#include <kernel/device.h>

struct tty_driver {
    struct file_operations *tty_ops;
};

struct tty {

};

#endif /* __TTY_H_ */
