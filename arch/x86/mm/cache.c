#include <arch/cache.h>

const char* cache_policy_string(cache_policy_t type)
{
    switch (type)
    {
        case CACHE_UC:       return "uncacheable";
        case CACHE_WC:       return "write-combining";
        case CACHE_WT:       return "writethrough";
        case CACHE_WP:       return "write-protect";
        case CACHE_WB:       return "writeback";
        case CACHE_UC_MINUS: return "uncached";
        default:             return "unknown";
    }
}
