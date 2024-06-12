#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    int  pipefd[2];
    char buf;
    int  cpid;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <string>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    cpid = fork();
    if (cpid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (cpid == 0) /* Child reads from pipe */
    {
        close(pipefd[1]);   /* Close unused write end */

        while (read(pipefd[0], &buf, 1) > 0)
        {
            write(STDOUT_FILENO, &buf, 1);
        }

        write(STDOUT_FILENO, "\n", 1);
        close(pipefd[0]);
        exit(EXIT_SUCCESS);

    }
    else /* Parent writes argv[1] to pipe */
    {
        close(pipefd[0]);          /* Close unused read end */
        write(pipefd[1], argv[1], strlen(argv[1]));
        close(pipefd[1]);          /* Reader will see EOF */
        int status;
        waitpid(-1, &status, 0);   /* Wait for child */
        exit(EXIT_SUCCESS);
    }
}
