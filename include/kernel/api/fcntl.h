#pragma once

#include <common/bits.h>
#include <kernel/api/types.h>

__BEGIN_DECLS

#define SEEK_SET    1
#define SEEK_CUR    2
#define SEEK_END    3

#define R_OK        4   /* Test for read permission */
#define W_OK        2   /* Test for write permission */
#define X_OK        1   /* Test for execute permission */
#define F_OK        0   /* Test for existence */

#define O_ACCMODE           3
#define O_RDONLY            0
#define O_WRONLY            1
#define O_RDWR              2
#define O_CREAT          0x40
#define O_EXCL           0x80
#define O_NOCTTY        0x100
#define O_TRUNC         0x200
#define O_APPEND        0x400
#define O_NONBLOCK      0x800
#define O_NOFOLLOW     0x1000
#define O_CLOEXEC      0x2000
#define O_DIRECTORY   0x10000

#define F_DUPFD             0 /* Duplicate file descriptor. */
#define F_GETFD             1 /* Get file descriptor flags. */
#define F_SETFD             2 /* Set file descriptor flags. */
#define F_GETFL             3 /* Get file status flags */
#define F_SETFL             4 /* Set file status flags */

#define FD_CLOEXEC 1

int fcntl(int fildes, int cmd, ...);
int open(const char*, int, ...);

__END_DECLS
