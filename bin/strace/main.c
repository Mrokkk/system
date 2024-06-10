#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_PATH_LEN 128

int main(int argc, char** argv)
{
    int pid, flag = 0;
    char pathname[MAX_PATH_LEN];
    const char* app = NULL;

    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp("-f", argv[i]))
        {
            flag = DTRACE_FOLLOW_FORK;
        }
        else if (!strchr(argv[i], '-'))
        {
            app = argv[i];
        }
        else
        {
            printf("Unrecognized option: %s\n", argv[i]);
            return EXIT_FAILURE;
        }
    }

    if (!app)
    {
        printf("Application path/name is needed\n");
        return EXIT_FAILURE;
    }

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


    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);

    if ((pid = fork()) == 0)
    {
        dtrace(flag);
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

