#include <kernel/fs.h>
#include <kernel/path.h>
#include <kernel/dentry.h>
#include <kernel/process.h>

static inline void get_next_dir(char** path, char* output)
{
    char* next_slash = strchr(*path, '/');

    if (!next_slash)
    {
        strcpy(output, *path);
        *path += strlen(*path);
    }
    else
    {
        memcpy(output, *path, next_slash - *path);
        output[next_slash - *path] = 0;
        *path = next_slash;
        ++*path;
    }
}

dentry_t* lookup(const char* filename)
{
    char name[256];
    char* path = (char*)filename;
    inode_t* parent_inode = NULL;
    dentry_t* parent_dentry = NULL;
    dentry_t* dentry = NULL;

    log_debug(DEBUG_LOOKUP, "called for filename=%S", filename);

    if (!path_is_absolute(filename))
    {
        dentry = process_current->fs->cwd;
        parent_inode = dentry->inode;
        parent_dentry = dentry;
        log_debug(DEBUG_LOOKUP, "relative; %O", dentry);
    }
    else if (process_current->fs->root)
    {
        dentry = process_current->fs->root;
        parent_inode = dentry->inode;
        parent_dentry = dentry;
        log_debug(DEBUG_LOOKUP, "absolute; %O", dentry);
        ++path;
    }
    else if (*filename == '/')
    {
        ++path;
    }

    while (1)
    {
        get_next_dir(&path, name);

        log_debug(DEBUG_LOOKUP, "dirname=%S", name);

        if (!strcmp(name, "."))
        {
        }
        else if (!strcmp(name, ".."))
        {
            dentry = dentry->parent
                ? dentry->parent
                : dentry;
        }
        else if (!strlen(name) && parent_dentry)
        {
            return parent_dentry;
        }
        else
        {
            dentry = dentry_lookup(dentry, name);

            if (unlikely(!dentry && !parent_inode))
            {
                return NULL;
            }
            else if (!dentry)
            {
                log_debug(DEBUG_LOOKUP, "calling ops->lookup with %O, %S", parent_inode, name);
                if (parent_inode->ops->lookup(parent_inode, name, &parent_inode))
                {
                    return NULL;
                }

                if (!parent_inode->dev)
                {
                    parent_inode->dev = parent_inode->sb->dev;
                }

                dentry = dentry_create(parent_inode, parent_dentry, name);
                parent_dentry = dentry;
            }
            else
            {
                parent_dentry = dentry;
                parent_inode = parent_dentry->inode;
            }
        }

        if (!path || strlen(path) == 0)
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
    }

    return dentry;
}
