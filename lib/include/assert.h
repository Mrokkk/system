#ifndef _ASSERT_H
#define _ASSERT_H

#ifdef __GNUC__
#define static_assert _Static_assert
#else
#define static_assert(...)
#endif

#endif

#undef assert

#ifndef NDEBUG

void __assertion_failed(const char* file, unsigned long line, const char* err);

#define assert(expr) \
    ((__builtin_expect(!(expr), 0)) \
        ? __assertion_failed(__FILE__, __LINE__, #expr) \
        : (void)0)

#else

#define assert(expr) ((void)0)

#endif
