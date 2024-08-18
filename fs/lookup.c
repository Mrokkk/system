#include <kernel/fs.h>
#include <kernel/path.h>
#include <kernel/dentry.h>
#include <kernel/process.h>
#include <kernel/api/stat.h>

static inline void get_next_dir(const char** path, char* output)
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

static int link_follow(dentry_t* dentry, dentry_t** result)
{
    int res, errno;
    char buffer[PATH_MAX];
    inode_t* inode;

    while (S_ISLNK(dentry->inode->mode))
    {
        inode = dentry->inode;

        if (unlikely(!inode->ops || !inode->ops->readlink))
        {
            return -ENOSYS;
        }

        res = inode->ops->readlink(inode, buffer, sizeof(buffer));

        if (unlikely(errno = errno_get(res)))
        {
            return errno;
        }

        buffer[res] = 0;

        if (unlikely(errno = lookup(buffer, LOOKUP_NOFOLLOW, &dentry)))
        {
            return errno;
        }
    }

    *result = dentry;

    return 0;
}

int lookup(const char* filename, int flag, dentry_t** result)
{
    int errno;
    char name[PATH_MAX];
    const char* path = filename;
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
            if (unlikely(!S_ISDIR(dentry->inode->mode)))
            {
                *result = NULL;
                return -ENOTDIR;
            }

            if (dentry != process_current->fs->root)
            {
                dentry = dentry->parent
                    ? dentry->parent
                    : dentry;
            }
        }
        else if (!*name && parent_dentry)
        {
            *result = parent_dentry;
            return 0;
        }
        else
        {
            dentry = dentry_lookup(dentry, name);

            if (unlikely(!dentry && !parent_inode))
            {
                *result = NULL;
                return -ENOENT;
            }
            else if (!dentry)
            {
                log_debug(DEBUG_LOOKUP, "calling ops->lookup with %O, %S", parent_inode, name);
                if (unlikely(errno = parent_inode->ops->lookup(parent_inode, name, &parent_inode)))
                {
                    *result = NULL;
                    return errno;
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
            break;
        }
        else if (S_ISLNK(dentry->inode->mode))
        {
            if (unlikely(errno = link_follow(dentry, &dentry)))
            {
                *result = NULL;
                return errno;
            }

            parent_inode = dentry->inode;
        }
    }

    if (unlikely(!dentry))
    {
        log_debug(DEBUG_LOOKUP, "returning %O", dentry);
        *result = NULL;
        return -ENOENT;
    }

    if (S_ISLNK(dentry->inode->mode) && (flag & LOOKUP_FOLLOW))
    {
        if (unlikely(errno = link_follow(dentry, &dentry)))
        {
            *result = NULL;
            return errno;
        }
    }

    log_debug(DEBUG_LOOKUP, "returning %O for dentry: %O", dentry->inode, dentry);

    *result = dentry;
    return 0;
}
