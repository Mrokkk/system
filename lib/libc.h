#pragma once

#define __LIBC

#define unlikely(x) __builtin_expect(!!(x), 0)
