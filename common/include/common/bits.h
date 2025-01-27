#pragma once

#ifndef __NORETURN

#if defined __GNUC__
#define __NORETURN [[noreturn]]
#elif defined __TINYC__
#define __NORETURN __attribute__((noreturn))
#else
#define __NORETURN
#endif

#endif
