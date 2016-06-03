#ifndef INCLUDE_KERNEL_TEST_H_
#define INCLUDE_KERNEL_TEST_H_

#include <kernel/debug.h>

#define ASSERT_FAILED(expected, achieved, sign) \
    do {                                                                \
        printk("%s FAILED!\n  ==> "                                     \
            #achieved" "#sign" "#expected" (0x%x "#sign" 0x%x)\n",      \
            __func__, (unsigned int)achieved,                           \
            (unsigned int)expected);                                    \
        *assert_failed = *assert_failed + 1;                            \
    } while(0)

#define ASSERT_EQ(var, value)                       \
    do {                                            \
        typeof(var) test = (typeof(var))(value);    \
        if (test != var) {                          \
            ASSERT_FAILED(value, var, ==);          \
        }                                           \
    } while (0)

#define ASSERT_NEQ(var, value)                      \
    do {                                            \
        typeof(var) test = (typeof(var))(value);    \
        if (test == var) {                          \
            ASSERT_FAILED(value, var, !=);          \
        }                                           \
    } while (0)

#define ASSERT_GT(var, value)                       \
    do {                                            \
        typeof(var) test = (typeof(var))(value);    \
        if (!(var > test)) {                        \
            ASSERT_FAILED(value, var, >);           \
        }                                           \
    } while (0)

#define ASSERT_GE(var, value)                       \
    do {                                            \
        typeof(var) test = (typeof(var))(value);    \
        if (!(var >= test)) {                       \
            ASSERT_FAILED(value, var, >=);          \
        }                                           \
    } while (0)

#define ASSERT_LT(var, value)                       \
    do {                                            \
        typeof(var) test = (typeof(var))(value);    \
        if (!(var < test)) {                        \
            ASSERT_FAILED(value, var, <);           \
        }                                           \
    } while (0)

#define ASSERT_LE(var, value)                       \
    do {                                            \
        typeof(var) test = (typeof(var))(value);    \
        if (!(var <= test)) {                       \
            ASSERT_FAILED(value, var, <);           \
        }                                           \
    } while (0)

#ifdef CONFIG_TESTS

#define TESTS_RUN()     \
    void tests_run();   \
    tests_run();        \
    while (1);

#else

#define TESTS_RUN()

#endif

#define TEST_CASE(name)             \
    auto void CASE_##name(int *);   \
    __assert_failed = 0;            \
    CASE_##name(&__assert_failed);  \
    if (__assert_failed)            \
        failed++;                   \
    else succeed++;                 \
    void CASE_##name(int *assert_failed)

#define TEST_SUITE(name)                                    \
    void test_suite_##name() {                              \
        int failed = 0, succeed = 0, __assert_failed = 0;

#define TEST_SUITE_END(name) \
    }

#endif /* INCLUDE_KERNEL_TEST_H_ */
