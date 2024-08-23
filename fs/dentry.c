#include <kernel/fs.h>
#include <kernel/dentry.h>
#include <kernel/malloc.h>

static LIST_DECLARE(dentry_cache);

static void dentry_init(dentry_t* dentry, dentry_t* parent_dentry, inode_t* inode)
{
    list_init(&dentry->cache);
    list_init(&dentry->child);
    list_init(&dentry->subdirs);
    list_add(&dentry->cache, &dentry_cache);
    dentry->refcount = 1;
    dentry->inode = inode;
    dentry->parent = parent_dentry;
}

dentry_t* dentry_create(inode_t* inode, dentry_t* parent_dentry, const char* name)
{
    size_t len;
    dentry_t* new_dentry = alloc(dentry_t, dentry_init(this, parent_dentry, inode));

    if (parent_dentry)
    {
        list_add(&new_dentry->child, &parent_dentry->subdirs);
    }

    len = strlen(name) + 1;
    new_dentry->name = slab_alloc(len);

    if (unlikely(!new_dentry->name))
    {
        delete(new_dentry);
        return NULL;
    }

    strcpy(new_dentry->name, name);
    inode->dentry = new_dentry;

    log_debug(DEBUG_DENTRY, "added %O for parent %O", new_dentry, parent_dentry);

    return new_dentry;
}

dentry_t* dentry_lookup(dentry_t* parent_dentry, const char* name)
{
    dentry_t* dentry;

    if (unlikely(!parent_dentry))
    {
        return NULL;
    }

    list_for_each_entry(dentry, &parent_dentry->subdirs, child)
    {
        if (!strcmp(dentry->name, name))
        {
            log_debug(DEBUG_DENTRY, "found %O", dentry);
            return dentry;
        }
    }

    return NULL;
}

void dentry_delete(dentry_t* dentry)
{
    if (!--dentry->refcount)
    {
        log_debug(DEBUG_DENTRY, "removing dentry %s %x", dentry->name, dentry->inode);
        list_del(&dentry->child);
        slab_free(dentry->name, strlen(dentry->name) + 1);
        delete(dentry);
    }
}
