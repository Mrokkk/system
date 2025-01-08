#pragma once

#include <kernel/api/signal.h>

#define WNOHANG                 1
#define WUNTRACED               2
#define WCONTINUED              4

#define WTERMSIG(status)        ((status) & 0x7f)
#define WIFEXITED(status)       (WTERMSIG(status) == 0)
#define WIFSIGNALED(status)     (((signed char) (((status) & 0x7f) + 1) >> 1) > 0)
#define WEXITSTATUS(status)     (((status) & 0xff00) >> 8)
#define WIFSTOPPED(status)      (((status) & 0xff) == 0x7f)

int waitpid(int pid, int* status, int options);
