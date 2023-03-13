#pragma once

#include <kernel/dev.h>
#include <kernel/types.h>

struct stat
{
    int st_mode;            // File mode
    ino_t st_ino;           // File serial number
    dev_t st_dev;           // Device containing the file
    //__uid_t st_uid;       // User ID of the file's owner
    //__gid_t st_gid;       // Group ID of the file's group
    size_t st_size;         // Size of file, in bytes
    //__time_t st_atime;    // Time of last access
    //__time_t st_mtime;    // Time of last modification
    //__time_t st_ctime;    // Time of last status change
};

int stat(const char* restrict pathname, struct stat* restrict statbuf);
