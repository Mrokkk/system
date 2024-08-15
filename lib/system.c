#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int LIBC(system)(const char* command)
{
    int status = (127 << 8);
    int res = fork();

    if (res == 0)
    {
        res = execlp("/bin/bash", "bash", "-c", command, NULL);
    }
    else if (res > 0)
    {
        waitpid(res, &status, 0);
    }

    return status;
}

LIBC_ALIAS(system);
