#pragma once

#include <sys/cdefs.h>

__BEGIN_DECLS

int mount(const char* source, const char* target, const char* fstype, int flags, void* data);
int umount(const char* target);
int umount2(const char* target, int flags);

__END_DECLS
