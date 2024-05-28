#include <kernel/fs.h>
#include <kernel/procfs.h>
#include <kernel/seq_file.h>

struct procfs_entry;
struct procfs_dir;

typedef struct procfs_entry procfs_entry_t;
typedef struct procfs_dir procfs_dir_t;

struct procfs_dir
{
    procfs_entry_t* entries;
    size_t count;
};

struct procfs_entry
{
    const char* name;
    size_t len;
    mode_t mode;
    inode_operations_t* iops;
    file_operations_t* fops;
};

typedef struct procfs_entry procfs_entry_t;
