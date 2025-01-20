#pragma once

enum cache_policy
{
    CACHE_UC       = 0,
    CACHE_WC       = 1,
    CACHE_RSVD1    = 2,
    CACHE_RSVD2    = 3,
    CACHE_WT       = 4,
    CACHE_WP       = 5,
    CACHE_WB       = 6,
    CACHE_UC_MINUS = 7,
    CACHE_MAX      = 7,
};

typedef enum cache_policy cache_policy_t;

const char* cache_policy_string(cache_policy_t type);
