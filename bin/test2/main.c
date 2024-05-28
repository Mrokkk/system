#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/stat.h>

#define BUFFER_SIZE 256

int main()
{
    int mousefd;
    char buffer[BUFFER_SIZE];
    if ((mousefd = open("/dev/mouse", O_RDONLY | O_NONBLOCK, 0)) == -1)
    {
        perror("/dev/mouse");
        return EXIT_FAILURE;
    }

    struct pollfd fds[2] = {
        {STDIN_FILENO, POLLIN, 0},
        {mousefd, POLLIN, 0}
    };

    while (1)
    {
        if (poll(fds, 2, -1) == -1)
        {
            perror("poll");
            return EXIT_FAILURE;
        }

        printf("poll finished\n");

        for (int i = 0; i < 2; ++i)
        {
            if (fds[i].revents == POLLIN)
            {
                int res = read(fds[i].fd, buffer, BUFFER_SIZE);
                printf("read %d from %u\n", res, fds[i].fd);
            }
        }
    }
    return 0;
}
