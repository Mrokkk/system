#pragma once

#include <stddef.h>

void* memset(void*, int, size_t);
void* memcpy(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);

size_t strlen(const char* s);
size_t strnlen(const char* s, size_t count);
char* strcpy(char* restrict, const char* restrict);
char* strncpy(char* restrict dest, const char* restrict src, size_t count);
int strcmp(const char*, const char*);
int strncmp(const char*, const char*, size_t);
int strcoll(const char* s1, const char* s2);
char* strchr(const char*, int);
char* strrchr(const char*, int);
char* strtok(char* restrict str, const char* restrict delim);
char* strtok_r(char* restrict str, const char* restrict delim, char** restrict saveptr);
size_t strspn(const char* s, const char* accept);
size_t strcspn(const char* s, const char* reject);

char* strerror(int errnum);
