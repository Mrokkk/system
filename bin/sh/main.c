#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define LINE_LEN        256
#define ARGV_MAX_SIZE   16
#define HISTORY_SIZE    32

char* history[HISTORY_SIZE];
int history_index = 0;

int c_cd();
int c_pwd();
int c_mkdir();
int c_history();

#define COMMAND(name) {#name, c_##name}

static struct {
    const char* name;
    int (*function)();
} commands[] = {
    COMMAND(cd),
    COMMAND(pwd),
    COMMAND(mkdir),
    COMMAND(history),
    {0, 0}
};

int sighan(int)
{
    printf("^C\n");
    return 0;
}

static inline void read_line(char* line, size_t size)
{
    size = read(0, line, size);
    if (size == 0)
    {
        exit(0);
    }
    line[size - 1] = 0;
}

static inline const char* word_read(const char* string, char* output)
{
    while (*string != '\n' && *string != '\0' && *string != ' ')
    {
        *output++ = *string++;
    }

    if (*string)
    {
        string++;
    }
    *output = 0;

    return string;
}

static inline void print_errno(const char* line, int errno)
{
    switch (errno)
    {
        case -EISDIR: printf("%s: is a directory\n", line); break;
        case -ENOENT: printf("%s: no such file or directory\n", line); break;
        case -EACCES: printf("%s: permission denied\n", line); break;
        case -ENOOPS: printf("%s: operation not possible\n", line); break;
        case -ERANGE: printf("%s: bad range\n", line); break;
        default: printf("%s: unknown error\n", line); break;
    }
}

int c_cd(const char* arg)
{
    if (!arg || !arg[0])
    {
        chdir("/");
        return 0;
    }
    return chdir(arg);
}

int c_pwd(const char*)
{
    char buf[32];
    int errno;

    if ((errno = getcwd(buf, 32)))
    {
        return errno;
    }

    printf("%s\n", buf);
    return 0;
}

int c_mkdir(const char* arg)
{
    return mkdir(arg, 0);
}

int c_history(const char*)
{
    for (int i = 0; i < history_index; ++i)
    {
        printf("%u %s\n", i, history[i]);
    }
    return 0;
}

static inline void command_prompt_print(int status)
{
    char cwd[128];
    getcwd(cwd, 128);
    printf(cwd);
    status ? printf(" * ") : printf(" # ");
}

static inline void cmd_args_read(char* command, int* argc, char* argv[], char* buffer[], const char* line)
{
    int i;
    const char* temp = word_read(line, command);

    for (*argc = 0; *argc < ARGV_MAX_SIZE && *temp; ++*argc)
    {
        temp = word_read(temp, buffer[*argc]);
    }

    argv[0] = command;
    for (i = 0; i < *argc; argv[i + 1] = buffer[i], ++i);
    argv[i + 1] = 0;
}

static inline int execute(const char* command, int argc, char* argv[], int* status)
{
    int errno, pid;
    (void)argc;

    for (int i = 0; commands[i].name; ++i)
    {
        if (!strcmp(commands[i].name, command))
        {
            if ((*status = commands[i].function(argv[1])))
            {
                print_errno(argv[0], *status);
            }
            return 0;
        }
    }

    if (command[0] != '/')
    {
        printf("No such command: %s\n", command);
        *status = 1;
        return 0;
    }

    if ((pid = fork()) == 0)
    {
        errno = exec(command, argv);
        print_errno(command, errno);
        exit(errno);
    }
    else if (pid < 0)
    {
        printf("%s: fork error!\n", command);
        *status = 1;
        return 1;
    }
    waitpid(pid, status, 0);

    if (WIFSIGNALED(*status) && WTERMSIG(*status) == SIGSEGV)
    {
        printf("Segmentation fault\n");
    }

    return 1;
}

int main()
{
    char line[LINE_LEN];
    char command[LINE_LEN];
    int argc, status = 0;
    char* arguments[ARGV_MAX_SIZE];
    char* argv[ARGV_MAX_SIZE];

    signal(SIGINT, sighan);

    for (int i = 0; i < HISTORY_SIZE; history[i++] = malloc(32));

    for (argc = 0; argc < ARGV_MAX_SIZE; arguments[argc++] = malloc(32));

    while (1)
    {
        command_prompt_print(status);

        read_line(line, LINE_LEN);

        if (line[0] == 0 || line[0] == '#')
        {
            continue;
        }

        strcpy(history[history_index++], line);
        // FIXME: use a better container than array
        if (history_index == HISTORY_SIZE) history_index = 0;

        cmd_args_read(command, &argc, argv, arguments, line);
        execute(command, argc, argv, &status);
    }

    return 0;
}
