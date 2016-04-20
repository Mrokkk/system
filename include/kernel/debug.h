#ifndef __DEBUG_H_
#define __DEBUG_H_

#include <kernel/compiler.h>

#if 1

    #include <kernel/string.h>

    #define ASSERT(cond)                                        \
        do {                                                    \
            int a = (int)(cond);                                \
            if (!a) {                                           \
                printk("%s:%d: %s: assertion '%s' failed\n",    \
                __FILENAME__, __LINE__, __func__, #cond);       \
            }                                                   \
        } while (0)

#else
    #define ASSERT(cond) do { } while(0)
#endif
    
    #define kernel_trace(string, ...)

#endif
