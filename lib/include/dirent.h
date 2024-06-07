#pragma once

#include <kernel/dirent.h>

typedef struct directory DIR;

DIR* opendir(const char* name);
DIR* fdopendir(int fd);

int closedir(DIR* dirp);

struct dirent* readdir(DIR* dirp);
