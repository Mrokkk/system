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

void dirent_print(struct dirent* dirent, struct stat* s)
{
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

    printf("%-10u %-10u %s%s"RESET"\n",
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
