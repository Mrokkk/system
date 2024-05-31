#pragma once

#include <stdio.h>

#define MOUNTED     "/etc/mtab"
#define MNTTAB      "/etc/fstab"

struct mntent
{
    char* mnt_fsname;
    char* mnt_dir;
    char* mnt_type;
    char* mnt_opts;
    int mnt_freq;
    int mnt_passno;
};

struct mntent* getmntent(FILE* stream);
FILE* setmntent(char const* filename, char const* type);
int endmntent(FILE* stream);
struct mntent* getmntent_r(FILE* stream, struct mntent* mntbuf, char* buf, int buflen);
