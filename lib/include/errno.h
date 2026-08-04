#pragma once

#include <sys/cdefs.h>
#include <kernel/api/errno.h>

__BEGIN_DECLS

extern int errno;
extern char* program_invocation_name;
extern char* program_invocation_short_name;

__END_DECLS
