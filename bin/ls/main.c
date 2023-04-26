#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_ENTRIES 100
#define BUFFER_SIZE 1024

#define DEVICE      "\033[35m"
#define DIRECTORY   "\033[34m"
#define RESET       "\033[0m"

static inline void permissions_fill(char* buffer, umode_t mode)
{
    *buffer++ = S_ISDIR(mode) ? 'd' : '-';
    *buffer++ = mode & S_IRUSR ? 'r' : '-';
    *buffer++ = mode & S_IWUSR ? 'w' : '-';
    *buffer++ = mode & S_IXUSR ? 'x' : '-';
    *buffer++ = mode & S_IRGRP ? 'r' : '-';
    *buffer++ = mode & S_IWGRP ? 'w' : '-';
    *buffer++ = mode & S_IXGRP ? 'x' : '-';
    *buffer++ = mode & S_IROTH ? 'r' : '-';
    *buffer++ = mode & S_IWOTH ? 'w' : '-';
    *buffer++ = mode & S_IXOTH ? 'x' : '-';
}

void dirent_print(struct dirent* dirent, struct stat* s)
{
    char buf[12];
    const char* type = "";
    int errno = stat(dirent->name, s);

    if (errno)
    {
        printf("error in stat for %s\n", dirent->name);
        return;
    }

    if (dirent->type == DT_CHR)
    {
        type = DEVICE;
    }
    else if (dirent->type == DT_DIR)
    {
        type = DIRECTORY;
    }

    permissions_fill(buf, s->st_mode);

    printf("%s %-5u %-5u %-10u %-10u %s%s"RESET"\n",
        buf,
        s->st_uid,
        s->st_gid,
        s->st_ino,
        s->st_size,
        type,
        dirent->name);
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
        printf("failed open %d\n", dirfd);
    }

    count = getdents(dirfd, buf, BUFFER_SIZE);

    if (count < 0)
    {
        printf("getdents failed %d\n", count);
        return -1;
    }

    dirent = (struct dirent*)buf;
    for (int i = 0; i < count; ++i)
    {
        dirent_print(dirent, stats + i);
        dirent = (struct dirent*)((char*)dirent + dirent->len);
    }

    return 0;
}
