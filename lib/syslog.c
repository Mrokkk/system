#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#define LINE_LEN 512

static char prefix[128];
static int options;

void openlog(const char* ident, int option, int facility)
{
    UNUSED(option);
    UNUSED(facility);

    char* it = prefix;

    it += sprintf(it, "%s", ident);

    if (option & LOG_PID)
    {
        it += sprintf(it, "[%u]", getpid());
    }

    it += sprintf(it, ": ");

    options = option;
}

void closelog(void)
{
}

void syslog(int priority, const char* format, ...)
{
    int count;
    char buffer[LINE_LEN];
    va_list args;

    count = sprintf(buffer, "%s", prefix);

    va_start(args, format);
    count = vsnprintf(buffer + count, LINE_LEN - count, format, args);
    va_end(args);

    buffer[LINE_LEN - 1] = '\0';

    _syslog(priority, options, buffer);
}
