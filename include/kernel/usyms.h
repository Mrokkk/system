#pragma once

#include <stdint.h>
#include <kernel/kernel.h>

struct user_symbol;
typedef struct user_symbol usym_t;

struct user_symbol
{
    char* name;
    uint32_t start;
    uint32_t end;
    usym_t* next;
};

usym_t* usym_find(uint32_t address, usym_t* symbols);
void usym_string(char* buffer, uint32_t addr, usym_t* symbols);
