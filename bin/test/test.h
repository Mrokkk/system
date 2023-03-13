#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define RED "\033[31m"
#define GREEN "\033[32m"
#define RESET "\033[0m"

struct test_case
{
    char* name;
    void (*test)(int*);
};

extern struct test_case test_cases[];

static inline const char* __argv_parse(int argc, char* argv[])
{
    for (int i = 0; i < argc; ++i)
    {
        unsigned len = strlen(argv[i]);
        if (len > 7)
        {
            if (!strncmp(argv[i], "--test=", 7))
            {
                return argv[i] + 7;
            }
        }
    }
    return NULL;
}

static inline int __tc_run(struct test_case* tc, const char* required_tc_name)
{
    int assert_failed = 0;
    if (required_tc_name && strcmp(required_tc_name, tc->name))
    {
        return -1;
    }
    printf(GREEN"[  RUN   ]"RESET" %s\n", tc->name);
    tc->test(&assert_failed);
    if (assert_failed)
    {
        printf(RED"[  FAIL  ]"RESET" %s\n", tc->name);
    }
    else
    {
        printf(GREEN"[  PASS  ]"RESET" %s\n", tc->name);
    }
    return assert_failed;
}

static inline void __verdict_print(int passed, int failed)
{
    printf(GREEN"[========]"RESET" Passed %u\n", passed);
    if (failed)
    {
        printf(RED"[========]"RESET" Failed %u\n", failed);
    }
}

static inline int __tcs_run(int argc, char* argv[], int count)
{
    int assert_failed, passed = 0, failed = 0;
    const char* test_name = __argv_parse(argc, argv);
    for (int i = 0; i < count; ++i)
    {
        assert_failed = __tc_run(&test_cases[i], test_name);
        if (assert_failed == -1) { continue; }
        else if (assert_failed == 0) { ++passed; }
        else { ++failed; }
    }
    __verdict_print(passed, failed);
    return failed;
}

#define EXPECT_FAILED(expected, actual, sign) \
    do { \
        printf( \
            RED"[========]"RESET" " __FILE__ ":%u\n" \
            RED"[ EXPECT ]"RESET" "#actual" "#sign" "#expected"\n" \
            RED"[ ACTUAL ]"RESET" "#actual" = %u (0x%x)\n", \
            __LINE__, (uint32_t)actual, (uint32_t)actual); \
        ++*assert_failed; \
    } while(0)

#define EXPECT_EQ(var, value) \
    do { \
        typeof(var) test = (typeof(var))(value); \
        if (test != (var)) { EXPECT_FAILED(value, var, ==); } \
    } while (0)

#define EXPECT_NE(var, value) \
    do { \
        typeof(var) test = (typeof(var))(value); \
        if (test == (var)) { EXPECT_FAILED(value, var, !=); } \
    } while (0)

#define EXPECT_GT(var, value) \
    do { \
        typeof(var) test = (typeof(var))(value); \
        if (!((var) > test)) { EXPECT_FAILED(value, var, >); } \
    } while (0)

#define EXPECT_GE(var, value) \
    do { \
        typeof(var) test = (typeof(var))(value); \
        if (!((var) >= test)) { EXPECT_FAILED(value, var, >=); } \
    } while (0)

#define EXPECT_LT(var, value) \
    do { \
        typeof(var) test = (typeof(var))(value); \
        if (!((var) < test)) { EXPECT_FAILED(value, var, <); } \
    } while (0)

#define EXPECT_LE(var, value) \
    do { \
        typeof(var) test = (typeof(var))(value); \
        if (!((var) <= test)) { EXPECT_FAILED(value, var, <); } \
    } while (0)

#define TEST_SUITE() \
    int __suite(int argc, char* argv[]) \
    { \

#define TEST_SUITE_END() \
        return __tcs_run(argc, argv, __COUNTER__); \
    } \
    struct test_case test_cases[__COUNTER__ - 1]

#define TEST(tc_name) \
    auto void CASE_##tc_name(int* assert_failed); \
    { \
        struct test_case* test_case = &test_cases[__COUNTER__]; \
        test_case->name = #tc_name; \
        test_case->test = &CASE_##tc_name; \
    } \
    void CASE_##tc_name(int* assert_failed)

#define TESTS_RUN(argc, argv) \
    __suite(argc, argv);
