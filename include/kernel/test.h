#ifndef INCLUDE_KERNEL_TEST_H_
#define INCLUDE_KERNEL_TEST_H_

#include <kernel/debug.h>

#define ASSERT_FAILED(name, expected, achieved, sign)               \
    do {                                                            \
        printk("[FAILED] %s:%d: %s(): assert "#name ": "            \
            #achieved" "#sign" "#expected" (0x%x "#sign" 0x%x)\n",  \
            __FILENAME__, __LINE__, __func__, achieved,             \
            expected);                                              \
    } while(0)

#define ASSERT_EQ(name, var, value)                 \
    do {                                            \
        typeof(var) test = (value);                 \
        if (test != var) {                          \
            ASSERT_FAILED(name, value, var, ==);    \
        }                                           \
    } while (0)

#define ASSERT_NEQ(name, var, value)                \
    do {                                            \
        typeof(var) test = (value);                 \
        if (test == var) {                          \
            ASSERT_FAILED(name, value, var, !=);    \
        }                                           \
    } while (0)

#define ASSERT_GT(name, var, value)                 \
    do {                                            \
        typeof(var) test = (value);                 \
        if (!(var > test)) {                        \
            ASSERT_FAILED(name, value, var, >);     \
        }                                           \
    } while (0)

#define ASSERT_LT(name, var, value)                 \
    do {                                            \
        typeof(var) test = (value);                 \
        if (!(var < test)) {                        \
            ASSERT_FAILED(name, value, var, <);     \
        }                                           \
    } while (0)

#define ASSERT_GE(name, var, value)                 \
    do {                                            \
        typeof(var) test = (value);                 \
        if (!(var >= test)) {                       \
            ASSERT_FAILED(name, value, var, >=);    \
        }                                           \
    } while (0)



#endif /* INCLUDE_KERNEL_TEST_H_ */
