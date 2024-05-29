#include <kernel/fs.h>
#include <kernel/dentry.h>

LIST_DECLARE(dentry_cache);

void dentry_init(dentry_t* dentry)
{
    list_init(&dentry->cache);
    list_init(&dentry->child);
    list_init(&dentry->subdirs);
    list_add(&dentry->cache, &dentry_cache);
    dentry->parent = NULL;
    dentry->inode = NULL;
#ifdef USE_MAGIC
    dentry->magic = DENTRY_MAGIC;
#endif
}

dentry_t* dentry_create(inode_t* inode, dentry_t* parent_dentry, const char* name)
{
    size_t len;
    dentry_t* new_dentry = alloc(dentry_t, dentry_init(this));

    if (parent_dentry)
    {
        list_add(&new_dentry->child, &parent_dentry->subdirs);
    }
    else if (!parent_dentry && inode != root)
    {
        log_debug(DEBUG_DENTRY, "null parent_dentry");
        delete(new_dentry);
        return NULL;
    }

    len = strlen(name) + 1;
    new_dentry->name = slab_alloc(len);
    new_dentry->inode = inode;
    new_dentry->parent = parent_dentry;
    strcpy(new_dentry->name, name);

    log_debug(DEBUG_DENTRY, "added %O for parent %O", new_dentry, parent_dentry);

    return new_dentry;
}

dentry_t* dentry_lookup(dentry_t* parent_dentry, const char* name)
{
    dentry_t* dentry;

    if (!parent_dentry)
    {
        list_for_each_entry(dentry, &dentry_cache, cache)
        {
            if (!strcmp(dentry->name, name))
            {
                log_debug(DEBUG_DENTRY, "found %O", dentry);
                return dentry;
            }
        }
    }
    else
    {
        list_for_each_entry(dentry, &parent_dentry->subdirs, child)
        {
            if (!strcmp(dentry->name, name))
            {
                log_debug(DEBUG_DENTRY, "found %O", dentry);
                return dentry;
            }
        }
    }

    return NULL;
}

dentry_t* dentry_get(struct inode* inode)
{
    dentry_t* dentry;
    list_for_each_entry(dentry, &dentry_cache, cache)
    {
        if (dentry->inode == inode)
        {
            return dentry;
        }
    }
    return NULL;
}

char* dentry_print(const void* data, char* str)
{
    const dentry_t* dentry = data;
    str += sprintf(str, "dentry_t{addr=%x, inode=%O, name=%S}", addr(dentry), dentry->inode, dentry->name);
    return str;
}
