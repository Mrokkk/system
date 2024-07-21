#pragma once

#include <stddef.h>
#include <stdint.h>

// https://pubs.opengroup.org/onlinepubs/009696899/basedefs/sys/types.h.html

typedef uint32_t    blkcnt_t;
typedef size_t      blksize_t;
typedef uint32_t    clock_t;
typedef uint32_t    clockid_t;
typedef uint32_t    dev_t;
typedef uint32_t    fsblkcnt_t;
typedef uint32_t    fsfilcnt_t;
typedef int         gid_t;
typedef uint32_t    id_t;
typedef uint32_t    ino_t;
typedef uint16_t    mode_t;
typedef uint32_t    nlink_t;
typedef size_t      off_t;
typedef uint16_t    pid_t;
typedef int32_t     ssize_t;
typedef int32_t     suseconds_t;
typedef uint32_t    useconds_t;
typedef uint32_t    time_t;
typedef uint32_t    timer_t;
typedef uint16_t    uid_t;

// Additional types
typedef uint8_t     pgid_t;
typedef size_t      daddr_t;
