#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <kernel/list.h>

#ifndef TEST_SUITES_COUNT
#define TEST_SUITES_COUNT       16
#endif
#define LIKELY(x)               __builtin_expect(!!(x), 1)
#define UNLIKELY(x)             __builtin_expect(!!(x), 0)
#define __STRINGIFY(x)          #x
#define STRINGIFY(x)            __STRINGIFY(x)

typedef enum comp comp_t;
typedef enum type type_t;
typedef struct value value_t;
typedef struct config config_t;
typedef struct test_case test_case_t;
typedef struct test_suite test_suite_t;

enum comp
{
    COMP_EQ,
    COMP_NE,
    COMP_GT,
    COMP_GE,
    COMP_LT,
    COMP_LE,
};

enum type
{
    TYPE_CHAR,
    TYPE_SHORT,
    TYPE_INT,
    TYPE_LONG,
    TYPE_LONG_LONG,
    TYPE_UNSIGNED_CHAR,
    TYPE_UNSIGNED_SHORT,
    TYPE_UNSIGNED_INT,
    TYPE_UNSIGNED_LONG,
    TYPE_UNSIGNED_LONG_LONG,
    TYPE_INT8_T,
    TYPE_INT16_T,
    TYPE_INT32_T,
    TYPE_INT64_T,
    TYPE_UINT8_T,
    TYPE_UINT16_T,
    TYPE_UINT32_T,
    TYPE_UINT64_T,
    TYPE_VOID_PTR,
    TYPE_CHAR_PTR,
    TYPE_SHORT_PTR,
    TYPE_INT_PTR,
    TYPE_LONG_PTR,
    TYPE_LONG_LONG_PTR,
    TYPE_UNSIGNED_CHAR_PTR,
    TYPE_UNSIGNED_SHORT_PTR,
    TYPE_UNSIGNED_INT_PTR,
    TYPE_UNSIGNED_LONG_PTR,
    TYPE_UNSIGNED_LONG_LONG_PTR,
    TYPE_INT8_T_PTR,
    TYPE_INT16_T_PTR,
    TYPE_INT32_T_PTR,
    TYPE_INT64_T_PTR,
    TYPE_UINT8_T_PTR,
    TYPE_UINT16_T_PTR,
    TYPE_UINT32_T_PTR,
    TYPE_UINT64_T_PTR,
    TYPE_UNRECOGNIZED,
};

struct value
{
    void* value;
    const char* name;
    type_t type;
};

struct config
{
    const char* test_to_run;
    bool run_until_failure;
    bool verbose;
};

struct test_case
{
    const char* name;
    void (*test)(void);
    list_head_t failed_tests;
    test_suite_t* suite;
};

struct test_suite
{
    const char* name;
    test_case_t* test_cases;
    size_t test_cases_count;
    int failed;
    int** assert_failed;
    config_t** config;
};

void __test_suite_register(test_suite_t* suite);
int __tcs_run(test_case_t* test_cases, int count, config_t* config);
int __test_suites_run(int argc, char* argv[]);

void failure_print(
    value_t* actual,
    value_t* expected,
    comp_t comp,
    const char* file,
    size_t line);

#define TYPE_GET(val) \
    _Generic((val), \
        char:               TYPE_CHAR, \
        short:              TYPE_SHORT, \
        int:                TYPE_INT, \
        long:               TYPE_LONG, \
        long long:          TYPE_LONG_LONG, \
        unsigned char:      TYPE_UNSIGNED_CHAR, \
        unsigned short:     TYPE_UNSIGNED_SHORT, \
        unsigned int:       TYPE_UNSIGNED_INT, \
        unsigned long:      TYPE_UNSIGNED_LONG, \
        unsigned long long: TYPE_UNSIGNED_LONG_LONG, \
        void*:              TYPE_VOID_PTR, \
        char*:              TYPE_CHAR_PTR, \
        short*:             TYPE_SHORT_PTR, \
        int*:               TYPE_INT_PTR, \
        long*:              TYPE_LONG_PTR, \
        long long*:         TYPE_LONG_LONG_PTR, \
        unsigned char*:     TYPE_UNSIGNED_CHAR_PTR, \
        unsigned short*:    TYPE_UNSIGNED_SHORT_PTR, \
        unsigned int*:      TYPE_UNSIGNED_INT_PTR, \
        unsigned long*:     TYPE_UNSIGNED_LONG_PTR, \
        unsigned long long*:TYPE_UNSIGNED_LONG_LONG_PTR, \
        default: \
    _Generic((val), \
        int8_t:             TYPE_INT8_T, \
        int16_t:            TYPE_INT16_T, \
        int32_t:            TYPE_INT32_T, \
        int64_t:            TYPE_UINT64_T, \
        uint8_t:            TYPE_UINT8_T, \
        uint16_t:           TYPE_UINT16_T, \
        uint32_t:           TYPE_UINT32_T, \
        uint64_t:           TYPE_UINT64_T, \
        int8_t*:            TYPE_INT8_T_PTR, \
        int16_t*:           TYPE_INT16_T_PTR, \
        int32_t*:           TYPE_INT32_T_PTR, \
        int64_t*:           TYPE_INT64_T_PTR, \
        uint8_t*:           TYPE_UINT8_T_PTR, \
        uint16_t*:          TYPE_UINT16_T_PTR, \
        uint32_t*:          TYPE_UINT32_T_PTR, \
        uint64_t*:          TYPE_UINT64_T_PTR, \
        default:            TYPE_UNRECOGNIZED))

#define EXPECT_FAILED(expected, expected_s, actual, actual_s, sign) \
    do \
    { \
        int type = TYPE_GET(actual); \
        value_t expected_value = { \
            .value = (void*)&expected, \
            .name = expected_s, \
            .type = type, \
        }; \
        value_t actual_value = { \
            .value = (void*)&actual, \
            .name = actual_s, \
            .type = type, \
        }; \
        failure_print(&actual_value, &expected_value, sign, __FILE__, __LINE__); \
        ++(*__assert_failed); \
    } \
    while(0)

#define EXPECT_GENERIC(C, SIGN, var, value) \
    do \
    { \
        typeof(var) actual = var; \
        typeof(var) expected = (typeof(var))(value); \
        if (UNLIKELY(!(actual SIGN expected))) \
        { \
            EXPECT_FAILED(expected, #value, actual, #var, COMP_##C); \
        } \
    } \
    while (0)

#define EXPECT_EQ(var, value) EXPECT_GENERIC(EQ, ==, var, value)
#define EXPECT_NE(var, value) EXPECT_GENERIC(NE, !=, var, value)
#define EXPECT_GT(var, value) EXPECT_GENERIC(GT, >, var, value)
#define EXPECT_GE(var, value) EXPECT_GENERIC(GE, >=, var, value)
#define EXPECT_LT(var, value) EXPECT_GENERIC(LT, <, var, value)
#define EXPECT_LE(var, value) EXPECT_GENERIC(LE, <=, var, value)

#define TEST_SUITE(n) \
    extern test_case_t __##n##_test_cases[]; \
    static test_case_t* test_cases = __##n##_test_cases; \
    static int* __assert_failed; \
    static config_t* __config; \
    static test_suite_t suite = { \
        .name = #n, \
        .test_cases = __##n##_test_cases, \
        .failed = 0, \
        .assert_failed = &__assert_failed, \
        .config = &__config, \
    }

#define TEST_SUITE_END(name) \
    static __attribute__((constructor)) void __##n##_ctor() \
    { \
        suite.test_cases_count = __COUNTER__; \
        __test_suite_register(&suite); \
    } \
    test_case_t __##name##_test_cases[__COUNTER__ - 1]

#define TEST(tc_name) \
    void CASE_##tc_name(void); \
    static __attribute__((constructor)) void __##tc_name##_ctor() \
    { \
        test_case_t* test_case = &test_cases[__COUNTER__]; \
        test_case->name = #tc_name; \
        test_case->test = &CASE_##tc_name; \
        test_case->suite = &suite; \
        list_init(&test_case->failed_tests); \
    } \
    void CASE_##tc_name(void)

#define FAILED_EXPECTATIONS() \
    ({ *__assert_failed; })

#define TESTS_RUN(argc, argv) \
    __test_suites_run(argc, argv)
