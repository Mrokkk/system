#include <kernel/vm.h>
#include <kernel/path.h>
#include <kernel/kernel.h>
#include <kernel/process.h>

int path_validate(const char* path)
{
    return current_vm_verify_string(VERIFY_READ, path);
}

int dirname_get(const char* path, char* output, size_t size)
{
    const char* last_slash = strrchr(path, '/');

    if (!last_slash)
    {
        if (unlikely(size < 1))
        {
            return -1;
        }

        *output = '\0';
        return 0;
    }

    if (unlikely(size <= (size_t)(last_slash - path)))
    {
        return -1;
    }

    memcpy(output, path, last_slash - path);
    output[last_slash - path] = 0;

    return 0;
}

typedef struct path_list path_list_t;

struct path_list
{
    const char*  name;
    path_list_t* next;
};

int path_construct(const dentry_t* dentry, char* output, size_t size)
{
    path_list_t* null = NULL;
    path_list_t* path;
    path_list_t** it = &null;
    path_list_t* next;
    char* temp = output;
    int i = 0;
    const char* end = output + size;

    while (dentry)
    {
        path = fmalloc(sizeof(*path));

        if (unlikely(!path))
        {
            goto no_memory;
        }

        path->name = dentry->name;
        path->next = *it;
        *it = path;

        if (dentry == process_current->fs->root)
        {
            path->name = "/";
            break;
        }

        dentry = dentry->parent;
    }

    path = *it;

    while (path)
    {
        if (temp - output + strlen(path->name) + 1 + (i > 1 ? 1 : 0) > size)
        {
            *output = 0;
            return -ENAMETOOLONG;
        }
        if (i > 1)
        {
            temp = csnprintf(temp, end, "/%s", path->name);
        }
        else
        {
            temp = csnprintf(temp, end, "%s", path->name);
        }
        next = path->next;
        ffree(path, sizeof(*path));
        path = next;
        ++i;
    }

    return 0;

no_memory:
    while (path)
    {
        next = path->next;
        ffree(path, sizeof(*path));
        path = next;
    }
    return -ENOMEM;
}
