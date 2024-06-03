#include <unistd.h>
#include <errno.h>

int pipe(int pipefd[2])
{
    UNUSED(pipefd);
    NOT_IMPLEMENTED(-1);
}
