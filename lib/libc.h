#pragma once

#define __LIBC

#ifndef __ASSEMBLER__

#include <error.h>
#include <errno.h>
#include <stdint.h>
#include <common/compiler.h>

#include "magic.h"

extern int libc_debug;

#define LIBC(name)              __libc_##name

#define STRONG_ALIAS(name, aliasname) \
    extern __typeof(name) aliasname __attribute__((alias(STRINGIFY(name))))

#define WEAK_ALIAS(name, aliasname) \
    extern __typeof(name) aliasname __attribute__((weak, alias(STRINGIFY(name))))

#define LIBC_ALIAS(name) \
    WEAK_ALIAS(LIBC(name), name)

#define ERRNO_SET(e, v) \
    ({ errno = e; v; })

#define DIR_CHECK(dir) \
    ({ (dir) && (dir)->magic == DIR_MAGIC; })

#define NOT_IMPLEMENTED(ret, fmt, ...) \
    ({ \
        if (UNLIKELY(libc_debug)) \
        { \
            error_at_line(0, ENOSYS, __FILE__, __LINE__, "%s(" fmt ")", __func__ + 7, __VA_ARGS__); \
        } \
        return ERRNO_SET(ENOSYS, ret); \
    })

#define NOT_IMPLEMENTED_NO_ARGS(ret) \
    NOT_IMPLEMENTED(ret, "", 0)

#define NOT_IMPLEMENTED_NO_RET(fmt, ...) \
    ({ \
        if (UNLIKELY(libc_debug)) \
        { \
            error_at_line(0, ENOSYS, __FILE__, __LINE__, "%s(" fmt ")", __func__ + 7, __VA_ARGS__); \
        } \
    })

#define NOT_IMPLEMENTED_NO_RET_NO_ARGS() \
    NOT_IMPLEMENTED_NO_RET("", 0)

#define RETURN_ERROR_IF(condition, err, ret) \
    if (UNLIKELY((condition))) return ERRNO_SET(err, ret)

#define VALIDATE_INPUT(condition, ret) \
    RETURN_ERROR_IF(!(condition), EINVAL, ret)

#define VALIDATE_SYSCALL_RETCODE(val, ret) \
    ({ if (UNLIKELY((int)(val) == -1)) return ret; val; })

#define VALIDATE_ALLOCATION(ptr, ret) \
    RETURN_ERROR_IF(!(ptr), ENOMEM, ret)

#define SAFE_ALLOC(ptr, ret) \
    ({ void* __x = ptr; VALIDATE_ALLOCATION(__x, ret); __x; })

#define SAFE_SYSCALL(val, ret) \
    ({ typeof(val) __x = (val); VALIDATE_SYSCALL_RETCODE(__x, ret); __x; })

#endif // __ASSEMBLER__
