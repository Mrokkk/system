#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_ENTRIES 100
#define BUFFER_SIZE 1024

#define DEVICE      "\033[35m"
#define DIRECTORY   "\033[34m"
#define RESET       "\033[0m"

static inline void permissions_fill(char* buffer, mode_t mode)
{
    if (S_ISDIR(mode))
    {
        *buffer++ = 'd';
    }
    else if (S_ISBLK(mode))
    {
        *buffer++ = 'b';
    }
    else if (S_ISCHR(mode))
    {
        *buffer++ = 'c';
    }
    else
    {
        *buffer++ = '-';
    }
    *buffer++ = mode & S_IRUSR ? 'r' : '-';
    *buffer++ = mode & S_IWUSR ? 'w' : '-';
    *buffer++ = mode & S_IXUSR ? 'x' : '-';
    *buffer++ = mode & S_IRGRP ? 'r' : '-';
    *buffer++ = mode & S_IWGRP ? 'w' : '-';
    *buffer++ = mode & S_IXGRP ? 'x' : '-';
    *buffer++ = mode & S_IROTH ? 'r' : '-';
    *buffer++ = mode & S_IWOTH ? 'w' : '-';
    *buffer++ = mode & S_IXOTH ? 'x' : '-';
    *buffer++ = 0;
}

void dirent_print(struct dirent* dirent, struct stat* s)
{
    char buf[12];
    const char* type = "";
    int errno = stat(dirent->d_name, s);

    if (errno)
    {
        perror(dirent->d_name);
        return;
    }

    if (dirent->d_type == DT_CHR)
    {
        type = DEVICE;
    }
    else if (dirent->d_type == DT_BLK)
    {
        type = DEVICE;
    }
    else if (dirent->d_type == DT_DIR)
    {
        type = DIRECTORY;
    }

    permissions_fill(buf, s->st_mode);

    printf("%s %-5u %-5u %-10u %-10lu %s%s"RESET"\n",
        buf,
        s->st_uid,
        s->st_gid,
        s->st_ino,
        s->st_size,
        type,
        dirent->d_name);
}

int main()
{
    char buf[BUFFER_SIZE];
    struct stat stats[MAX_ENTRIES];
    struct dirent* dirent;
    int dirfd, count;

    getcwd(buf, BUFFER_SIZE);

    dirfd = open(buf, O_RDONLY | O_DIRECTORY, 0);

    if (dirfd < 0)
    {
        perror(buf);
        return EXIT_FAILURE;
    }

    count = getdents(dirfd, buf, BUFFER_SIZE);

    if (count < 0)
    {
        perror(buf);
        return EXIT_FAILURE;
    }

    dirent = (struct dirent*)buf;
    for (int i = 0; i < count; ++i)
    {
        dirent_print(dirent, stats + i);
        dirent = (struct dirent*)((char*)dirent + dirent->d_reclen);
    }

    return 0;
}
