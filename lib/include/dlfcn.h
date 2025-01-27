#pragma once

// The MODE argument to `dlopen' contains one of the following:
#define RTLD_LAZY           0x00001 // Lazy function call binding.
#define RTLD_NOW            0x00002 // Immediate function call binding.
#define RTLD_BINDING_MASK   0x00003 // Mask of binding time value.
#define RTLD_NOLOAD         0x00004 // Do not load the object.
#define RTLD_DEEPBIND       0x00008 // Use deep binding.

// If the following bit is set in the MODE argument to `dlopen',
// the symbols of the loaded object and its dependencies are made
// visible as if the object were linked directly into the program.
#define RTLD_GLOBAL         0x00100

// Unix98 demands the following flag which is the inverse to RTLD_GLOBAL.
// The implementation does this by default and so we can define the
// value to zero.
#define RTLD_LOCAL          0

// Do not delete object when closed.
#define RTLD_NODELETE       0x01000

// If the first argument of `dlsym' or `dlvsym' is set to RTLD_NEXT
// the run-time address of the symbol called NAME in the next shared
// object is returned.  The "next" relation is defined by the order
// the shared objects were loaded.
#define RTLD_NEXT   ((void *) -1l)

// If the first argument to `dlsym' or `dlvsym' is set to RTLD_DEFAULT
// the run-time address of the symbol called NAME in the global scope
// is returned.
#define RTLD_DEFAULT    ((void *) 0)

void* dlopen(const char* filename, int flags);
int dlclose(void* handle);
void* dlsym(void* restrict handle, const char* restrict symbol);
