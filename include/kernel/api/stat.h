#pragma once

#include <kernel/api/dev.h>
#include <kernel/api/types.h>

#define S_IFMT  00170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

#define S_IRUGO     (S_IRUSR|S_IRGRP|S_IROTH)
#define S_IWUGO     (S_IWUSR|S_IWGRP|S_IWOTH)
#define S_IXUGO     (S_IXUSR|S_IXGRP|S_IXOTH)

struct stat
{
    mode_t  st_mode;    // File mode
    ino_t   st_ino;     // File serial number
    dev_t   st_dev;     // Device containing the file
    nlink_t st_nlink;   // Number of hard links to the file
    dev_t   st_rdev;    // Device ID (if file is character or block special)
    uid_t   st_uid;     // User ID of the file's owner
    gid_t   st_gid;     // Group ID of the file's group
    size_t  st_size;    // Size of file, in bytes
    time_t  st_atime;   // Time of last access
    time_t  st_mtime;   // Time of last modification
    time_t  st_ctime;   // Time of last status change
};

int stat(const char* restrict pathname, struct stat* restrict statbuf);
int fstat(int fd, struct stat* statbuf);
int chmod(const char* pathname, mode_t mode);
int fchmod(int fd, mode_t mode);
mode_t umask(mode_t cmask);
int mknod(const char* pathname, mode_t mode, dev_t dev);
