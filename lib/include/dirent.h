#pragma once

#include <sys/cdefs.h>
#include <kernel/api/dirent.h>

__BEGIN_DECLS

typedef struct directory DIR;

DIR* opendir(const char* name);
DIR* fdopendir(int fd);

int closedir(DIR* dirp);

struct dirent* readdir(DIR* dirp);
void rewinddir(DIR* dirp);

__END_DECLS
