#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_PATH_LEN 128

int main(int argc, char** argv)
{
    int pid;

    if (argc < 2)
    {
        printf("Application path/name is needed\n");
        return EXIT_FAILURE;
    }

    char pathname[MAX_PATH_LEN];
    const char* app = argv[1];

    if (strlen(app) >= MAX_PATH_LEN)
    {
        printf("Name too long: %s\n", app);
        return EXIT_FAILURE;
    }

    if (strchr(app, '/'))
    {
        strcpy(pathname, app);
    }
    else
    {
        strcpy(pathname, "/bin/");
        strcat(pathname, app);
    }

    char* args[] = {pathname, NULL};

    if ((pid = fork()) == 0)
    {
        dtrace();
        execvp(pathname, args);
        perror(pathname);
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        perror("fork error");
        return EXIT_FAILURE;
    }

    int status;
    waitpid(pid, &status, WUNTRACED);

    return 0;
}

