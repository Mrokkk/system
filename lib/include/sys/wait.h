#pragma once

int waitpid(int pid, int* status, int options);

#define WTERMSIG(status)        ((status) & 0x7f)
#define WIFEXITED(status)       (WTERMSIG(status) == 0)
#define WIFSIGNALED(status)     (((signed char) (((status) & 0x7f) + 1) >> 1) > 0)
#define WEXITSTATUS(status)     (((status) & 0xff00) >> 8)
