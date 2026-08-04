#pragma once

#include <sys/cdefs.h>
#include <kernel/api/syslog.h>

__BEGIN_DECLS

void syslog(int priority, const char* format, ...);
void openlog(const char* ident, int option, int facility);
void closelog(void);

__END_DECLS
