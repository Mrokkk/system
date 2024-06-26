#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel/fs.h>
#include <kernel/list.h>
#include <kernel/binary.h>

typedef struct arg arg_t;
typedef struct aux aux_t;
typedef struct argvec argvec_t;

typedef struct binfmt binfmt_t;

struct arg
{
    size_t len;
    char* arg;
    list_head_t list_entry;
};

struct aux
{
    int type;
    uintptr_t value;
    list_head_t list_entry;
};

struct argvec
{
    list_head_t head;
    int count;
    size_t size;
};

typedef argvec_t argvecs_t[3];

struct binfmt
{
    const char* name;
    char signature;
    int (*prepare)(const char* name, file_t* file, string_t** interpreter, void** data);
    int (*load)(file_t* file, binary_t* bin, void* data, argvecs_t argvecs);
    int (*interp_load)(file_t* file, binary_t* bin, void* data, argvecs_t argvecs);
    void (*cleanup)(void* data);
    list_head_t list_entry;
};

#define ARGV 0
#define ENVP 1
#define AUXV 2

#define ARGVECS_DECLARE(name) \
    argvecs_t name = { \
        {.head = LIST_INIT(name[0].head)}, \
        {.head = LIST_INIT(name[1].head)}, \
        {.head = LIST_INIT(name[2].head)}, \
    }

#define ARGVECS_SIZE(name) \
    ({ align((name)[ARGV].size + (name)[ENVP].size + (name)[AUXV].size, sizeof(uintptr_t)); })

int binfmt_register(binfmt_t* binfmt);

#define FRONT 1
#define TAIL  0

// arg_insert - allocate and insert new arg at front/tail of specific argvec
//
// @string - string arg
// @len - length of arg including '\0'
// @argvec - argvec to which new arg is inserted
// @flag - whetner new entry should be added at front or tail
int arg_insert(const char* string, const size_t len, argvec_t* argvec, int flag);

static inline int envp_insert(const char* string, const size_t len, argvecs_t argvecs)
{
    return arg_insert(string, len, &argvecs[ENVP], TAIL);
}

static inline int argv_insert(const char* string, const size_t len, argvecs_t argvecs, int flag)
{
    return arg_insert(string, len, &argvecs[ARGV], flag);
}

int aux_insert(int type, uintptr_t value, argvecs_t argvecs);
