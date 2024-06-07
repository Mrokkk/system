#pragma once

#define __LIBC

#ifndef __ASSEMBLER__

#include <errno.h>
#include <stdint.h>
#include "magic.h"

#define UNUSED(x)   (void)x
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define LIBC(name)      __libc_##name
#define __STRINGIFY(x)  #x
#define STRINGIFY(x)    __STRINGIFY(x)
#define ADDR(a)         ((uintptr_t)(a))
#define PTR(a)          ((void*)(a))

#define STRONG_ALIAS(name, aliasname) \
  extern __typeof(name) aliasname __attribute__((alias(STRINGIFY(name))));

#define WEAK_ALIAS(name, aliasname) \
  extern __typeof(name) aliasname __attribute__((weak, alias(STRINGIFY(name))));

#define ERRNO_SET(e, v) \
    ({ errno = e; v; })

#define ALIGN_TO(address, size) \
    (((address) + (size) - 1) & (~((size) - 1)))

#define FILE_CHECK(file) \
    ({ (file) && (file)->magic == FILE_MAGIC; })

#define DIR_CHECK(dir) \
    ({ (dir) && (dir)->magic == DIR_MAGIC; })

#define NOT_IMPLEMENTED(ret) \
    return ERRNO_SET(ENOSYS, ret);

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

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;

#endif // __ASSEMBLER__
