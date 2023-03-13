#pragma once

#ifdef __GNUC__
typedef __builtin_va_list   __gnuc_va_list;
typedef __gnuc_va_list      va_list;
#define va_start(v,l)   __builtin_va_start(v,l)
#define va_end(v)       __builtin_va_end(v)
#define va_arg(v,l)     __builtin_va_arg(v,l)
#define va_copy(d,s)    __builtin_va_copy(d,s)
#else
#error "Not a GCC"
#endif
