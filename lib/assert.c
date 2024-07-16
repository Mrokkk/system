#include <assert.h>

void __assertion_failed(const char* file, unsigned long line, const char* err)
{
    error_at_line(0, 0, file, line, "assertion failed: %s", err);
}
