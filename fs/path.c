#include <kernel/vm.h>
#include <kernel/path.h>
#include <kernel/kernel.h>
#include <kernel/process.h>

int path_validate(const char* path)
{
    // FIXME: do proper validation
    return current_vm_verify(VERIFY_READ, path);
}

void dirname(const char* path, char* output)
{
    const char* last_slash = find_last_slash(path);

    if (last_slash - path == 0)
    {
        strcpy(output, "/");
        return;
    }

    memcpy(output, path, last_slash - path);
    output[last_slash - path] = 0;
}

struct path_list
{
    const char* name;
    struct path_list* next;
};

int path_construct(dentry_t* dentry, char* output, size_t size)
{
    struct path_list* null = NULL;
    struct path_list* path;
    struct path_list** it = &null;
    struct path_list* next;
    char* temp = output;
    int i = 0;

    while (dentry)
    {
        path = fmalloc(sizeof(struct path_list));
        path->name = dentry->name;
        path->next = *it;
        *it = path;
        dentry = dentry->parent;
    }

    path = *it;

    while (path)
    {
        if (temp - output + strlen(path->name) + 1 + (i > 1 ? 1 : 0) > size)
        {
            *output = 0;
            return -ERANGE;
        }
        if (i > 1)
        {
            temp += sprintf(temp, "/%s", path->name);
        }
        else
        {
            temp += sprintf(temp, "%s", path->name);
        }
        next = path->next;
        ffree(path, sizeof(struct path_list));
        path = next;
        ++i;
    }

    return 0;
}
