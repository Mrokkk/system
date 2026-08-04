#pragma once

#ifndef __NORETURN

#if defined(__GNUC__) || defined(__TINYC__)
#define __NORETURN __attribute__((noreturn))
#else
#define __NORETURN
#endif

#endif

#ifdef __cplusplus
#define __BEGIN_DECLS   extern "C" {
#define __END_DECLS     }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

#if __STDC_VERSION__ < 199901L
#define restrict
#endif
