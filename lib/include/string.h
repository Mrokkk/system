#pragma once

#include <stddef.h>

void* memset(void* s, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
void* mempcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);

void* memchr(const void* s, int c, size_t n);
void* memrchr(const void* s, int c, size_t n);

size_t strlen(const char* s);
size_t strnlen(const char* s, size_t count);
char* stpcpy(char* restrict dst, const char* restrict src);
char* strcpy(char* restrict dst, const char* restrict src);
char* stpncpy(char* dst, const char* src, size_t size);
char* strncpy(char* restrict dst, const char* restrict src, size_t size);
size_t strlcpy(char* dst, const char* src, size_t size);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
int strcoll(const char* s1, const char* s2);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
char* strtok(char* restrict str, const char* restrict delim);
char* strtok_r(char* restrict str, const char* restrict delim, char** restrict saveptr);
size_t strspn(const char* s, const char* accept);
size_t strcspn(const char* s, const char* reject);
char* strstr(const char* haystack, const char* needle);
char* strcat(char* restrict dst, const char* restrict src);
char* strncat(char* restrict dst, const char* restrict src, size_t ssize);
char* strpbrk(const char* s, const char* accept);
char* strdup(const char* s);
size_t strxfrm(char* dest, char const* src, size_t n);

void explicit_bzero(void* s, size_t n);

char* strerror(int errnum);
char* strsignal(int sig);
const char *sigdescr_np(int sig);
const char* sigabbrev_np(int sig);
