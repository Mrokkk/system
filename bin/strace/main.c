#include <stdio.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char** argv)
{
    int pid, flag = 0;
    char* app_argv[32] = {NULL};
    size_t app_argc = 0;
    int res;

    while ((res = getopt(argc, argv, "-fb")) != -1)
    {
        switch (res)
        {
            case 'f':
                flag |= DTRACE_FOLLOW_FORK;
                break;

            case 'b':
                flag |= DTRACE_BACKTRACE;
                break;

            case 1:
                if (app_argc == sizeof(app_argv) / sizeof(*app_argv))
                {
                    fprintf(stderr, "Too many arguments\n");
                    exit(EXIT_FAILURE);
                }
                app_argv[app_argc++] = optarg;
                break;

            default:
                return -1;
        }
    }

    if (!app_argv[0])
    {
        fprintf(stderr, "Application path/name is needed\n");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);

    if ((pid = fork()) == 0)
    {
        if (dtrace(flag))
        {
            perror("dtrace");
            exit(EXIT_FAILURE);
        }
        execvp(app_argv[0], app_argv);
        perror(app_argv[0]);
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
