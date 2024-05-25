#pragma once

#include <stdint.h>

#define U16(a, b) \
    (((a) << 0) | ((b) << 8))

#define U32(a, b, c, d) \
    (((a) << 0) | ((b) << 8) | ((c) << 16) | ((d) << 24))

#define U32_ORDER_SWAP(value) \
    ({ \
        ((((value)  >> 24)  & 0x000000ff) | \
        (((value)   >> 8)   & 0x0000ff00) | \
        (((value)   << 8)   & 0x00ff0000) | \
        (((value)   << 24)  & 0xff000000)); \
    })

typedef struct u16_lsb u16_lsb_t;
typedef struct u16_msb u16_msb_t;

typedef struct u32_lsb u32_lsb_t;
typedef struct u32_msb u32_msb_t;

struct u16_lsb
{
    uint8_t byte0;
    uint8_t byte1;
};

struct u16_msb
{
    uint8_t byte1;
    uint8_t byte0;
};

struct u32_lsb
{
    uint8_t byte0;
    uint8_t byte1;
    uint8_t byte2;
    uint8_t byte3;
};

struct u32_msb
{
    uint8_t byte3;
    uint8_t byte2;
    uint8_t byte1;
    uint8_t byte0;
};

struct u64_lsb
{
    uint8_t byte0;
    uint8_t byte1;
    uint8_t byte2;
    uint8_t byte3;
    uint8_t byte4;
    uint8_t byte5;
    uint8_t byte6;
    uint8_t byte7;
};

struct u64_msb
{
    uint8_t byte7;
    uint8_t byte6;
    uint8_t byte5;
    uint8_t byte4;
    uint8_t byte3;
    uint8_t byte2;
    uint8_t byte1;
    uint8_t byte0;
};

#define NATIVE_U16(value) \
    ({ \
        ((value).byte0 | \
        ((value).byte1 << 8)); \
    })

#define NATIVE_U32(value) \
    ({ \
        ((value).byte0 | \
        ((value).byte1 << 8) | \
        ((value).byte2 << 16) | \
        ((value).byte3 << 24)); \
    })

#define NATIVE_U64(value) \
    ({ \
        ((uint64_t)(value).byte0 | \
        ((uint64_t)(value).byte1 << 8) | \
        ((uint64_t)(value).byte2 << 16) | \
        ((uint64_t)(value).byte3 << 24) | \
        ((uint64_t)(value).byte4 << 32) | \
        ((uint64_t)(value).byte5 << 40) | \
        ((uint64_t)(value).byte6 << 48) | \
        ((uint64_t)(value).byte7 << 56)); \
    })
