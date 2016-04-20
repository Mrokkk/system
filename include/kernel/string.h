#ifndef __STRING_H_
#define __STRING_H_

#include <arch/string.h>
#include <kernel/types.h>

extern size_t strlen(const char *s);
extern size_t strnlen(const char *s, size_t count);
extern char *strcpy(char *__restrict, const char *__restrict);

extern int strcmp(const char *, const char *);

extern int strncmp(const char *, const char *, size_t);
extern char *strchr(const char *, int);
extern char *strrchr(const char *, int);
extern void bcopy(const void *, void *, size_t);
extern void *memcpy(void *dest, const void *src, size_t n);
extern void *memcpyw(unsigned short *, const unsigned short *, size_t);
extern void *memset(void *, int, size_t);
extern void *memsetw(unsigned short *, unsigned short, size_t);

#endif /* __STRING_H_ */
