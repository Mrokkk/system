#pragma once

#ifndef __NORETURN

#if defined(__GNUC__) || defined(__TINYC__)
#define __NORETURN __attribute__((noreturn))
#else
#define __NORETURN
#endif

#endif
