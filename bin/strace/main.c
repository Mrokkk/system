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
    char* app_argv[32] = {NULL};
    size_t app_argc = 1;

    for (int i = 1; i < argc; ++i)
    {
        if (!app && !strcmp("-f", argv[i]))
        {
            flag = DTRACE_FOLLOW_FORK;
        }
        else if (!strchr(argv[i], '-'))
        {
            if (!app)
            {
                app = argv[i];
            }
            else
            {
                app_argv[app_argc++] = argv[i];
            }
        }
        else
        {
            app_argv[app_argc++] = argv[i];
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

    app_argv[0] = pathname;

    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);

    if ((pid = fork()) == 0)
    {
        dtrace(flag);
        execvp(pathname, app_argv);
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

