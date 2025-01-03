#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel/kernel.h>

#define SYMBOL_NAME_SIZE 32

struct ksym;
typedef struct ksym ksym_t;

struct ksym
{
    uintptr_t address;
    uint16_t size;
    char type;
    ksym_t* next;
    char name[0];
};

ksym_t* ksym_find_by_name(const char *name);
ksym_t* ksym_find(uintptr_t address);
int ksyms_load(void* start, void* end);
void ksym_string(uintptr_t addr, char* buffer, size_t size);
