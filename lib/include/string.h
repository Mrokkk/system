#pragma once

#include <stddef.h>

void* memset(void*, int, size_t);
void* memcpy(void* dest, const void* src, size_t n);

size_t strlen(const char* s);
size_t strnlen(const char* s, size_t count);
char* strcpy(char* restrict, const char* restrict);
char* strncpy(char* restrict dest, const char* restrict src, size_t count);
int strcmp(const char*, const char*);
int strncmp(const char*, const char*, size_t);
char* strchr(const char*, int);
char* strrchr(const char*, int);
