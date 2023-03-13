#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_ENTRIES 10

void dirent_print(struct dirent* dirent, struct stat* s)
{
    int errno = stat(dirent->name, s);
    if (errno)
    {
        printf("error in stat for %s\n", dirent->name);
        return;
    }

    printf("%-10u %-10u %s\n",
        s->st_ino,
        s->st_size,
        dirent->name);
}

int main()
{
    char cwd[128];
    struct dirent dirents[MAX_ENTRIES];
    struct stat stats[MAX_ENTRIES];
    int dirfd, count;

    getcwd(cwd, 128);

    dirfd = open(cwd, 0, 0);

    if (dirfd < 0)
    {
        printf("failed open %d\n", dirfd);
    }

    count = getdents(dirfd, dirents, MAX_ENTRIES);

    if (count < 0)
    {
        printf("getdents failed %d\n", count);
        return -1;
    }

    for (int i = 0; i < count; ++i)
    {
        if (strlen(dirents[i].name) == 0)
        {
            break;
        }
        dirent_print(dirents + i, stats + i);
    }

    return 0;
}
