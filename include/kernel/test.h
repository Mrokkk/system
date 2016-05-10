#ifndef INCLUDE_KERNEL_TEST_H_
#define INCLUDE_KERNEL_TEST_H_

#include <kernel/debug.h>

#define ASSERT_FAILED(name, expected, achieved, sign) \
    do {                                                                \
        printk("%s FAILED!\n  ==> "#name ": "               \
            #achieved" "#sign" "#expected" (0x%x "#sign" 0x%x)\n",      \
            __func__, (unsigned int)achieved, \
            (unsigned int)expected);                                    \
        *assert_failed = *assert_failed + 1;                            \
    } while(0)

#define ASSERT_EQ(name, var, value)                 \
    do {                                            \
        typeof(var) test = (typeof(var))(value);    \
        if (test != var) {                          \
            ASSERT_FAILED(name, value, var, ==);    \
        }                                           \
    } while (0)

#define ASSERT_NEQ(name, var, value)                \
    do {                                            \
        typeof(var) test = (typeof(var))(value);    \
        if (test == var) {                          \
            ASSERT_FAILED(name, value, var, !=);    \
        }                                           \
    } while (0)

#define ASSERT_GT(name, var, value)                 \
    do {                                            \
        typeof(var) test = (typeof(var))(value);    \
        if (!(var > test)) {                        \
            ASSERT_FAILED(name, value, var, >);     \
        }                                           \
    } while (0)

#define ASSERT_GE(name, var, value)                 \
    do {                                            \
        typeof(var) test = (typeof(var))(value);    \
        if (!(var >= test)) {                       \
            ASSERT_FAILED(name, value, var, >=);    \
        }                                           \
    } while (0)

#define ASSERT_LT(name, var, value)                 \
    do {                                            \
        typeof(var) test = (typeof(var))(value);    \
        if (!(var < test)) {                        \
            ASSERT_FAILED(name, value, var, <);     \
        }                                           \
    } while (0)

#define ASSERT_LE(name, var, value)                 \
    do {                                            \
        typeof(var) test = (typeof(var))(value);    \
        if (!(var <= test)) {                        \
            ASSERT_FAILED(name, value, var, <);     \
        }                                           \
    } while (0)

#ifdef CONFIG_TESTS

#define TESTS_RUN() \
    void tests_run(); \
    tests_run();

#else

#define TESTS_RUN()

#endif

#define TEST_SUITE(name) \
    auto void SUITE_##name(int *);\
    __assert_failed = 0; \
    SUITE_##name(&__assert_failed); \
    if (__assert_failed) \
        failed++;   \
    else succeed++; \
    void SUITE_##name(int *assert_failed)

#define TEST_PLAN(name) \
    void test_plan_##name() {    \
        int failed = 0, succeed = 0, __assert_failed = 0;

#define TEST_PLAN_END(name) \
        printk("\nTest plan '"#name"': %d succeed, %d failed\n", succeed, failed);  \
    }

#endif /* INCLUDE_KERNEL_TEST_H_ */
