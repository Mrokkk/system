#include <kernel/kernel.h>
#include <kernel/backtrace.h>

void memory_dump_impl(const char* severity, const uint32_t* addr, size_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        log_severity(severity, "%08x: %08x", addr + i, addr[i]);
    }
}
