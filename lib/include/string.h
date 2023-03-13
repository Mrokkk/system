#pragma once

#include <stddef.h>

size_t strlen(const char* s);
size_t strnlen(const char* s, size_t count);
char* strcpy(char* restrict, const char* restrict);
char* strncpy(char* restrict dest, const char* restrict src, size_t count);
int strcmp(const char*, const char*);
int strncmp(const char*, const char*, size_t);
char* strchr(const char*, int);
char* strrchr(const char*, int);
void* memcpy(void* dest, const void* src, size_t n);
void* memcpyw(unsigned short*, const unsigned short*, size_t);
void* memset(void*, int, size_t);
void* memsetw(unsigned short*, unsigned short, size_t);
