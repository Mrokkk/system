#include <kernel/kernel.h>
#include <kernel/unistd.h>

/*===========================================================================*
 *                                 read_line                                 *
 *===========================================================================*/
static inline void read_line(char *line) {

    int size = read(0, line, 32);
    line[size-1] = 0;

}

/*===========================================================================*
 *                                  c_zombie                                 *
 *===========================================================================*/
static int c_zombie() {

    int pid = fork();

    exit(pid);
    return 0;
}

/*===========================================================================*
 *                                   c_bug                                   *
 *===========================================================================*/
static int c_bug() {

    printf("Bug!!!\n");
    kill(getppid(), SIGINT);
    exit(0);
    return 0;

}

#define COMMAND(name) {#name, c_##name}

static struct command {
    char *name;
    int (*function)();
} commands[] = {
        COMMAND(zombie),
        COMMAND(bug),
        {0, 0}
};

/*===========================================================================*
 *                                  sighan                                   *
 *===========================================================================*/
int sighan(int pid) {
    printf("Got signal from %d\n", pid);
    return 0;
}

/*===========================================================================*
 *                                temp_shell                                 *
 *===========================================================================*/
int temp_shell() {

    char line[32];
    struct command *com = commands;
    int i, pid, status = 0;

    signal(SIGINT, sighan);

    while (1) {
        status ? printf("* ") : printf("# ");
        read_line(line);
        if (line[0] == 0) continue;
        if ((pid=fork()) == 0) {
            for (i=0; com[i].name; i++) {
                if (!strcmp(com[i].name, line))
                    exec(com[i].function);
            }
            if (!com[i].name) printf("No such command: %s\n", line);
            exit(-EBADC);
        } else if (pid < 0) printf("%s: fork error!\n", line);
        waitpid(pid, &status, 0);
    }

    exit(0);

}
