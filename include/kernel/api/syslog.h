#pragma once

// Facilities
#define LOG_USER    (1 << 8)

// Options
#define LOG_CONS    (1 << 9)
#define LOG_PID     (1 << 10)

#define LOG_INFO    1
#define LOG_NOTICE  2
#define LOG_WARNING 3
#define LOG_ERR     4
#define LOG_CRIT    5

int _syslog(int priority, int option, const char* message);
