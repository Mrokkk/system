#pragma once

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define RED "\033[31m"
#define GREEN "\033[32m"
#define RESET "\033[0m"

struct test_case
{
    char* name;
    void (*test)(int*);
    int failed;
};

extern struct test_case test_cases[];

#define __RUN_UNTIL_FAIL 2

static inline const char* __argv_parse(int argc, char* argv[], int* flags)
{
    const char* tc_name = NULL;
    for (int i = 0; i < argc; ++i)
    {
        unsigned len = strlen(argv[i]);
        if (len == 9)
        {
            if (!strncmp(argv[i], "--forever", 9))
            {
                *flags = __RUN_UNTIL_FAIL;
            }
        }
        else if (len > 7)
        {
            if (!strncmp(argv[i], "--test=", 7))
            {
                tc_name = argv[i] + 7;
            }
        }
        else if (len == 2)
        {
            if (!strncmp(argv[i], "-f", 2))
            {
                *flags = __RUN_UNTIL_FAIL;
            }
        }
    }
    return tc_name;
}

static inline int __tc_run(struct test_case* tc)
{
    int assert_failed = 0;

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

static inline void __verdict_print(int passed, int failed, int count)
{
    struct test_case* tc;

    printf(GREEN"[========]"RESET" Passed %u\n", passed);
    if (failed)
    {
        printf(RED"[========]"RESET" Failed %u\n", failed);
        for (int i = 0; i < count; ++i)
        {
            tc = test_cases + i;
            if (tc->failed)
            {
                printf(RED"[========]"RESET" %s\n", tc->name);
            }
        }
    }
}

static inline int __tcs_run(int argc, char* argv[], int count)
{
    int assert_failed = 0, passed = 0, failed = 0, flags = 0;
    const char* test_name = __argv_parse(argc, argv, &flags);
    struct test_case* tc;

    for (int i = 0; i < count; ++i)
    {
        tc = test_cases + i;
        if (test_name && strcmp(test_name, tc->name))
        {
            continue;
        }

        if (test_name && flags & __RUN_UNTIL_FAIL)
        {
            while (!(assert_failed = __tc_run(tc)));
        }
        else
        {
            assert_failed = __tc_run(tc);
        }

        if (assert_failed == 0)
        {
            ++passed;
        }
        else
        {
            ++failed;
            tc->failed = 1;
        }
    }

    __verdict_print(passed, failed, count);
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
        test_case->failed = 0; \
    } \
    void CASE_##tc_name(int* assert_failed)

#define TESTS_RUN(argc, argv) \
    __suite(argc, argv);
