#pragma once

#include <kernel/api/syslog.h>

void syslog(int priority, const char* format, ...);
void openlog(const char* ident, int option, int facility);
void closelog(void);
