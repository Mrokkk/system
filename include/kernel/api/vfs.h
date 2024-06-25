#pragma once

#include <kernel/api/types.h>

typedef unsigned long __fsword_t;
typedef unsigned long fsid_t;

struct statfs
{
    __fsword_t f_type;    // Type of filesystem
    __fsword_t f_bsize;   // Optimal transfer block size
    fsblkcnt_t f_blocks;  // Total data blocks in filesystem
    fsblkcnt_t f_bfree;   // Free blocks in filesystem
    fsblkcnt_t f_bavail;  // Free blocks available to unprivileged user
    fsfilcnt_t f_files;   // Total inodes in filesystem
    fsfilcnt_t f_ffree;   // Free inodes in filesystem
    fsid_t     f_fsid;    // Filesystem ID
    __fsword_t f_namelen; // Maximum length of filenames
    __fsword_t f_frsize;  // Fragment size
    __fsword_t f_flags;   // Mount flags of filesystem
};

int statfs(const char* path, struct statfs* buf);
int fstatfs(int fd, struct statfs* buf);
