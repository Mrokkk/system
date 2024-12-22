#pragma once

#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <common/list.h>
#include <common/compiler.h>

#ifndef TEST_SUITES_COUNT
#define TEST_SUITES_COUNT       16
#endif

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
    void*       value;
    const char* name;
    type_t      type;
};

struct config
{
    const char* test_to_run;
    bool        run_until_failure;
    bool        verbose;
    bool        quiet;
};

struct test_case
{
    const char*     name;
    list_head_t     failed_tests;
    test_suite_t*   suite;

    void (*test)(void);
};

struct test_suite
{
    const char*  name;
    test_case_t* test_cases;
    size_t       test_cases_count;
    int          failed;
    config_t**   config;
};

void __test_suite_register(test_suite_t* suite);
int __tcs_run(test_case_t* test_cases, int count, config_t* config);
int __test_suites_run(int argc, char* argv[]);
int expect_exit_with(int pid, int expected_error_code, const char* file, size_t line);
int expect_killed_by(int pid, int signal, const char* file, size_t line);

void string_check(
    value_t* actual,
    value_t* expected,
    const char* file,
    size_t line);

void failure_print(
    value_t* actual,
    value_t* expected,
    comp_t comp,
    const char* file,
    size_t line);

void user_failure_print(const char* file, size_t line, const char* fmt, ...);

extern int __assert_failed;

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

#define EXPECT_FAILED(expected, expected_s, actual, actual_s, sign, file, line) \
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
        failure_print(&actual_value, &expected_value, sign, file, line); \
        ++__assert_failed; \
    } \
    while(0)

#define EXPECT_GENERIC(C, SIGN, l, r, file, line) \
    do \
    { \
        typeof(l) actual = l; \
        typeof(l) expected = (typeof(l))(r); \
        if (UNLIKELY(!(actual SIGN expected))) \
        { \
            EXPECT_FAILED(expected, #r, actual, #l, COMP_##C, file, line); \
        } \
    } \
    while (0)

#define EXPECT_STR_EQ(l, r) \
    do \
    { \
        const char* actual = (const char*)(l); \
        const char* expected = (const char*)(r); \
        value_t expected_value = { \
            .value = (void*)&expected, \
            .name = #r, \
            .type = TYPE_CHAR_PTR, \
        }; \
        value_t actual_value = { \
            .value = (void*)&actual, \
            .name = #l, \
            .type = TYPE_CHAR_PTR, \
        }; \
        string_check(&actual_value, &expected_value, __FILE__, __LINE__); \
    } \
    while (0)

#define EXPECT_EQ_L(l, r, file, line) EXPECT_GENERIC(EQ, ==, l, r, file, line)
#define EXPECT_NE_L(l, r, file, line) EXPECT_GENERIC(NE, !=, l, r, file, line)
#define EXPECT_GT_L(l, r, file, line) EXPECT_GENERIC(GT, >,  l, r, file, line)
#define EXPECT_GE_L(l, r, file, line) EXPECT_GENERIC(GE, >=, l, r, file, line)
#define EXPECT_LT_L(l, r, file, line) EXPECT_GENERIC(LT, <,  l, r, file, line)
#define EXPECT_LE_L(l, r, file, line) EXPECT_GENERIC(LE, <=, l, r, file, line)

#define EXPECT_EQ(l, r) EXPECT_EQ_L(l, r, __FILE__, __LINE__)
#define EXPECT_NE(l, r) EXPECT_NE_L(l, r, __FILE__, __LINE__)
#define EXPECT_GT(l, r) EXPECT_GT_L(l, r, __FILE__, __LINE__)
#define EXPECT_GE(l, r) EXPECT_GE_L(l, r, __FILE__, __LINE__)
#define EXPECT_LT(l, r) EXPECT_LT_L(l, r, __FILE__, __LINE__)
#define EXPECT_LE(l, r) EXPECT_LE_L(l, r, __FILE__, __LINE__)

#define __GUARD_INIT(pid) \
    ({ if (pid == 0) __assert_failed = 0; 0; })

#define __FORK_FOR() \
    for (int pid = fork(), __guard = __GUARD_INIT(pid); !__guard; ++__guard) \

#define EXPECT_EXIT_WITH_L(expected_error_code, file, line) \
    __FORK_FOR() \
        if (pid > 0) \
        { \
            __assert_failed += expect_exit_with(pid, expected_error_code, file, line); \
        } \
        else

#define EXPECT_EXIT_WITH(expected_error_code) EXPECT_EXIT_WITH_L(expected_error_code, __FILE__, __LINE__)

#define EXPECT_KILLED_BY_L(signal, file, line) \
    __FORK_FOR() \
        if (pid > 0) \
        { \
            __assert_failed += expect_killed_by(pid, signal, file, line); \
        } \
        else

#define EXPECT_KILLED_BY(signal) EXPECT_KILLED_BY_L(signal, __FILE__, __LINE__)

#define FAIL_L(file, line, fmt, ...) \
    ({ __assert_failed++; user_failure_print(file, line, fmt, ##__VA_ARGS__); 0; })

#define FAIL(fmt, ...) \
    FAIL_L(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define __TEST_SKIPPED (1 << 31)

#define SKIP() \
    ({ __assert_failed = __TEST_SKIPPED; return; 0; })

#define SKIP_WHEN(condition) \
    ({ bool c = condition; if (UNLIKELY(c)) { SKIP(); } 0; })

#define TEST_SUITE(n) \
    extern test_case_t __##n##_test_cases[]; \
    static test_case_t* test_cases = __##n##_test_cases; \
    static config_t* __config; \
    static test_suite_t suite = { \
        .name = #n, \
        .test_cases = __##n##_test_cases, \
        .failed = 0, \
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
    ({ __assert_failed; })

#define TESTS_RUN(argc, argv) \
    __test_suites_run(argc, argv)
