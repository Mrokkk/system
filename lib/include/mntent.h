#pragma once

#include <stdio.h>

// File listing canonical interesting mount points
#define MNTTAB      "/etc/fstab"
// File listing currently active mount points
#define MOUNTED     "/etc/mtab"

// General filesystem types
#define MNTTYPE_IGNORE      "ignore"    // Ignore this entry
#define MNTTYPE_NFS         "nfs"       // Network file system
#define MNTTYPE_SWAP        "swap"      // Swap device

// Generic mount options
#define MNTOPT_DEFAULTS     "defaults"  // Use all default options
#define MNTOPT_RO           "ro"        // Read only
#define MNTOPT_RW           "rw"        // Read/write
#define MNTOPT_SUID         "suid"      // Set uid allowed
#define MNTOPT_NOSUID       "nosuid"    // No set uid allowed
#define MNTOPT_NOAUTO       "noauto"    // Do not auto mount

struct mntent
{
    char*   mnt_fsname;
    char*   mnt_dir;
    char*   mnt_type;
    char*   mnt_opts;
    int     mnt_freq;
    int     mnt_passno;
};

struct mntent* getmntent(FILE* stream);
FILE* setmntent(char const* filename, char const* type);
int endmntent(FILE* stream);
struct mntent* getmntent_r(FILE* stream, struct mntent* mntbuf, char* buf, int buflen);
