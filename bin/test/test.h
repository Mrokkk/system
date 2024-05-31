#pragma once

#include <stdio.h>
#include <stdbool.h>

#define TEST_SUITES_COUNT       16
#define LIKELY(x)               __builtin_expect(!!(x), 1)
#define UNLIKELY(x)             __builtin_expect(!!(x), 0)
#define __STRINGIFY(x)          #x
#define STRINGIFY(x)            __STRINGIFY(x)

typedef struct config config_t;
typedef struct test_case test_case_t;
typedef struct test_suite test_suite_t;

struct test_case
{
    const char* name;
    void (*test)(int* assert_failed, config_t* config);
    int failed;
};

struct test_suite
{
    const char* name;
    test_case_t* test_cases;
    size_t test_cases_count;
    int failed;
};

struct config
{
    const char* test_to_run;
    bool run_forever;
    bool verbose;
};

extern const char* const GEN_RED_MSG;
extern const char* const EXPECT_MSG;
extern const char* const ACTUAL_MSG;

void __test_suite_register(test_suite_t* suite);
int __tcs_run(test_case_t* test_cases, int count, config_t* config);
int __test_suites_run(int argc, char* argv[]);

#define VALUE_FORMAT(val) \
    _Generic((val), \
        uint32_t:   "%u", \
        uint16_t:   "%u", \
        uint8_t:    "%u", \
        int:        "%d", \
        short:      "%d", \
        char:       "%c", \
        default:    "0x%lx")

#define VALUE(val) \
    _Generic((val), \
        uint32_t:   (val), \
        uint16_t:   (val), \
        uint8_t:    (val), \
        int:        (val), \
        short:      (val), \
        char:       (val), \
        default:    (unsigned long)(val))

#define EXPECT_FAILED(expected, expected_s, actual, actual_s, sign) \
    do { \
        char __buf[1024]; \
        char* __it = __buf; \
        __it += sprintf(__it, "%s " __FILE__ ":%u\n", GEN_RED_MSG, __LINE__); \
        __it += sprintf(__it, "%s " actual_s " " sign " " expected_s " (", EXPECT_MSG); \
        __it += sprintf(__it, VALUE_FORMAT(expected), VALUE(expected)); \
        __it += sprintf(__it, ")\n"); \
        __it += sprintf(__it, "%s " actual_s " = ", ACTUAL_MSG); \
        __it += sprintf(__it, VALUE_FORMAT(actual), VALUE(actual)); \
        __it += sprintf(__it, "\n"); \
        fputs(__buf, stdout); \
        ++*assert_failed; \
    } while(0)

#define EXPECT_EQ(var, value) \
    do { \
        typeof(var) test = (typeof(var))(value); \
        typeof(var) actual = var; \
        if (UNLIKELY(test != actual)) \
        { \
            EXPECT_FAILED(value, #value, actual, #var, STRINGIFY(==)); \
        } \
    } while (0)

#define EXPECT_NE(var, value) \
    do { \
        typeof(var) test = (typeof(var))(value); \
        typeof(var) actual = var; \
        if (UNLIKELY(test == actual)) \
        { \
            EXPECT_FAILED(value, #value, actual, #var, STRINGIFY(!=)); \
        } \
    } while (0)

#define EXPECT_GT(var, value) \
    do { \
        typeof(var) test = (typeof(var))(value); \
        typeof(var) actual = var; \
        if (UNLIKELY(!(actual > test))) \
        { \
            EXPECT_FAILED(value, #value, actual, #var, STRINGIFY(>)); \
        } \
    } while (0)

#define EXPECT_GE(var, value) \
    do { \
        typeof(var) test = (typeof(var))(value); \
        typeof(var) actual = var; \
        if (UNLIKELY(!(actual >= test))) \
        { \
            EXPECT_FAILED(value, #value, actual, #var, STRINGIFY(>=)); \
        } \
    } while (0)

#define EXPECT_LT(var, value) \
    do { \
        typeof(var) test = (typeof(var))(value); \
        typeof(var) actual = var; \
        if (UNLIKELY(!(actual < test))) \
        { \
            EXPECT_FAILED(value, #value, actual, #var, STRINGIFY(<)); \
        } \
    } while (0)

#define EXPECT_LE(var, value) \
    do { \
        typeof(var) test = (typeof(var))(value); \
        typeof(var) actual = var; \
        if (UNLIKELY(!(actual <= test))) \
        { \
            EXPECT_FAILED(value, #value, actual, #var, STRINGIFY(<)); \
        } \
    } while (0)

#define TEST_SUITE(n) \
    extern test_case_t __##n##_test_cases[]; \
    static test_case_t* test_cases = __##n##_test_cases; \
    static test_suite_t suite = { \
        .name = #n, \
        .test_cases = __##n##_test_cases, \
        .failed = 0, \
    }

#define TEST_SUITE_END(name) \
    static __attribute__((constructor)) void __##n##_ctor() \
    { \
        suite.test_cases_count = __COUNTER__; \
        __test_suite_register(&suite); \
    } \
    test_case_t __##name##_test_cases[__COUNTER__ - 1]

#define TEST(tc_name) \
    void CASE_##tc_name(int* assert_failed, config_t* config); \
    static __attribute__((constructor)) void __##tc_name##_ctor() \
    { \
        test_case_t* test_case = &test_cases[__COUNTER__]; \
        test_case->name = #tc_name; \
        test_case->test = &CASE_##tc_name; \
        test_case->failed = 0; \
    } \
    void CASE_##tc_name(int* assert_failed, config_t*)

#define TESTS_RUN(argc, argv) \
    __test_suites_run(argc, argv)
