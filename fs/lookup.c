#include <kernel/fs.h>
#include <kernel/path.h>
#include <kernel/dentry.h>
#include <kernel/process.h>

static inline void get_next_dir(const char* path, char* output)
{
    char* first_slash = strchr(path, '/');

    if (first_slash == path)
    {
        strcpy(output, "/");
        return;
    }

    if (!first_slash)
    {
        strcpy(output, path);
    }
    else
    {
        memcpy(output, path, first_slash - path);
        output[first_slash - path] = 0;
    }
}

dentry_t* lookup(const char* filename)
{
    char name[256];
    char* path = (char*)filename;
    struct inode* parent_inode = NULL;
    dentry_t* parent_dentry = NULL;
    dentry_t* dentry = NULL;

    log_debug(DEBUG_LOOKUP, "called for filename=%S", filename);

    // If filename is relative, get cwd
    if (!path_is_absolute(filename))
    {
        dentry = process_current->fs->cwd;
        parent_inode = dentry->inode;
        parent_dentry = dentry;
        log_debug(DEBUG_LOOKUP, "relative; %O", dentry);
    }

    while (1)
    {
        get_next_dir(path, name);

        log_debug(DEBUG_LOOKUP, "dirname=%S", name);

        if (!strcmp(name, ".."))
        {
            dentry = dentry->parent;
            goto next;
        }

        dentry = dentry_lookup(dentry, name);

        log_debug(DEBUG_LOOKUP, "got dentry %O", dentry);

        if (unlikely(!dentry && !parent_inode))
        {
            return NULL;
        }
        else if (!dentry)
        {
            // There's an assumption that there is a dentry for root, so we may enter here only during second run
            log_debug(DEBUG_LOOKUP, "calling ops->lookup with %O, %S", parent_inode, name);
            if (parent_inode->ops->lookup(parent_inode, name, &parent_inode))
            {
                return NULL;
            }
            dentry = dentry_create(parent_inode, parent_dentry, name);
            parent_dentry = dentry;
        }
        else
        {
            parent_dentry = dentry;
            parent_inode = parent_dentry->inode;
        }

next:
        log_debug(DEBUG_LOOKUP, "calling path_next with %S", path);
        path = path_next(path);

        if (!path || strlen(path) == 1)
        {
            if (dentry)
            {
                log_debug(DEBUG_LOOKUP, "returning %O for dentry: %O", dentry->inode, dentry);
            }
            else
            {
                log_debug(DEBUG_LOOKUP, "returning %O", dentry);
            }
            return dentry;
        }
        path++;
    }

    return dentry;
}
