#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel/fs.h>
#include <kernel/list.h>

typedef struct arg arg_t;
typedef struct aux aux_t;
typedef struct binfmt binfmt_t;
typedef struct argvec argvec_t;
typedef struct aux_data aux_data_t;

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
    uintptr_t* ptr;
};

struct aux_data
{
    size_t size;
    const void* data;
    list_head_t list_entry;
    aux_t* aux;
};

struct argvec
{
    list_head_t head;
    int count;
    size_t size;
    const void* data;
    size_t data_size;
};

#define ARGV     0
#define ENVP     1
#define AUXV     2
#define AUX_DATA 3

#define ARGVECS_DECLARE(name) \
    argvecs_t name = { \
        [ARGV] = {.head = LIST_INIT(name[ARGV].head)}, \
        [ENVP] = {.head = LIST_INIT(name[ENVP].head)}, \
        [AUXV] = {.head = LIST_INIT(name[AUXV].head)}, \
        [AUX_DATA] = {.head = LIST_INIT(name[AUX_DATA].head)}, \
    }

#define ARGVECS_SIZE(name) \
    ({ \
        align((name)[ARGV].size + (name)[ENVP].size + (name)[AUXV].size, sizeof(uintptr_t)) + \
        align((name)[AUX_DATA].size, sizeof(uintptr_t)); \
    })

typedef argvec_t argvecs_t[4];

typedef struct binary binary_t;

struct binary
{
    void* entry;
    uintptr_t brk;
    uintptr_t code_start;
    uintptr_t code_end;
};

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

aux_t* aux_insert(int type, uintptr_t value, argvecs_t argvecs);
