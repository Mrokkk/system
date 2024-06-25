#pragma once

#include <stddef.h>
#include <kernel/fs.h>
#include <kernel/list.h>
#include <kernel/binary.h>

typedef struct binfmt binfmt_t;
typedef struct interp interp_t;

// TODO: make a string_t
struct interp
{
    char* data;
    short len;
};

struct binfmt
{
    const char* name;
    char signature;
    int (*prepare)(const char* name, file_t* file, interp_t** interpreter, void** data);
    int (*load)(file_t* file, binary_t* bin, void* data);
    void (*cleanup)(void* data);
    list_head_t list_entry;
};

struct arg
{
    size_t len;
    char* arg;
    list_head_t list_entry;
};

struct argvec
{
    list_head_t head;
    int count;
    size_t size;
};

typedef struct arg arg_t;
typedef struct argvec argvec_t;

#define ARGV 0
#define ENVP 1

typedef argvec_t argvecs_t[2];

#define ARGVECS_DECLARE(name) \
    argvecs_t name = { \
        {.head = LIST_INIT(name[0].head)}, \
        {.head = LIST_INIT(name[1].head)}, \
    }

#define ARGVECS_SIZE(name) \
    ({ (name)[ARGV].size + (name)[ENVP].size; })

int binfmt_register(binfmt_t* binfmt);
