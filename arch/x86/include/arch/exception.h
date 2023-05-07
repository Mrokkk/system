#ifndef __EXCEPTION_H_
#define __EXCEPTION_H_

#define PF_PRESENT  1
#define PF_WRITE    2
#define PF_USER     4
#define PF_RESERVED 8
#define PF_IFETCH   16

#endif

#ifndef __exception
#define __exception(...)
#endif

#ifndef __exception_debug
#define __exception_debug(...)
#endif

#define ERROR_CODE      1
#define NO_ERROR_CODE   0

#define GENERAL_PROTECTION  13
#define PAGE_FAULT          14

__exception(divide_error,       0,      NO_ERROR_CODE,  "division by zero",             SIGFPE)
__exception_debug(debug,        1,      NO_ERROR_CODE,  "debug",                        SIGTRAP)
__exception(nmi,                2,      NO_ERROR_CODE,  "non maskable interrupt",       0)
__exception(breakpoint,         3,      NO_ERROR_CODE,  "breakpoint",                   SIGTRAP)
__exception(overflow,           4,      NO_ERROR_CODE,  "into detected overflow",       SIGSEGV)
__exception(bound_range,        5,      NO_ERROR_CODE,  "out of bounds",                SIGSEGV)
__exception(invalid_opcode,     6,      NO_ERROR_CODE,  "invalid opcode",               SIGILL)
__exception(device_na,          7,      NO_ERROR_CODE,  "no coprocessor",               0)
__exception(double_fault,       8,      ERROR_CODE,     "double fault",                 SIGSEGV)
__exception(coprocessor,        9,      NO_ERROR_CODE,  "coprocessor segment overrun",  SIGFPE)
__exception(invalid_tss,        10,     ERROR_CODE,     "bad tss",                      SIGSEGV)
__exception(segment_np,         11,     ERROR_CODE,     "segment not present",          SIGBUS)
__exception(stack_segment,      12,     ERROR_CODE,     "stack fault",                  SIGBUS)
__exception(general_protection, 13,     ERROR_CODE,     "general protection fault",     SIGSEGV)
__exception(page_fault,         14,     ERROR_CODE,     "page fault",                   SIGSEGV)
