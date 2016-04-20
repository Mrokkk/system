#ifndef __DIRENT_H_
#define __DIRENT_H_

#include <kernel/types.h>

struct dirent {
    int d_ino;
    size_t d_off;
    size_t d_len;
    char d_type;
    char name[256];
};

#endif /* __DIRENT_H_ */
