#define log_fmt(fmt) "procfs: " fmt
#include <kernel/fs.h>
#include <kernel/init.h>
#include <kernel/path.h>
#include <kernel/time.h>
#include <kernel/memory.h>
#include <kernel/minmax.h>
#include <kernel/procfs.h>
#include <kernel/process.h>
#include <kernel/seq_file.h>
#include <kernel/vm_print.h>
#include <kernel/api/dirent.h>
#include <kernel/page_table.h>
#include <kernel/generic_vfs.h>

#define DEBUG_PROCFS 0

static int procfs_mount(super_block_t* sb, inode_t* inode, void*, int);
static int procfs_open(file_t* file);
static int procfs_close(file_t* file);

static int procfs_root_lookup(inode_t* dir, const char* name, inode_t** result);
static int procfs_root_readdir(file_t* file, void* buf, direntadd_t dirent_add);

static int procfs_pid_lookup(inode_t* dir, const char* name, inode_t** result);
static int procfs_pid_readlink(inode_t* inode, char* buffer, size_t size);
static int procfs_pid_readdir(file_t* file, void* buf, direntadd_t dirent_add);

static int procfs_fd_lookup(inode_t* dir, const char* name, inode_t** result);
static int procfs_fd_readdir(file_t* file, void* buf, direntadd_t dirent_add);
static int procfs_fd_readlink(inode_t* inode, char* buffer, size_t size);

static int comm_show(seq_file_t* s);
static int status_show(seq_file_t* s);
static int stack_show(seq_file_t* s);
static int meminfo_show(seq_file_t* s);
static int cmdline_show(seq_file_t* s);
static int uptime_show(seq_file_t* s);
static int environ_show(seq_file_t* s);
int syslog_show(seq_file_t* s);
int maps_show(seq_file_t* s);

typedef struct procfs_pid_data procfs_pid_data_t;

struct procfs_pid_data
{
    list_head_t entries;
};

#define READDIR_END_OFFSET ((size_t)-1)

#define PROCFS_ENTRY(name) \
    static int name##_open(file_t* file) { return seq_open(file, &name##_show); } \
    static inode_operations_t name##_iops; \
    static file_operations_t name##_fops = { \
        .open = &name##_open, \
        .close = &seq_close, \
        .read = &seq_read, \
    }

static file_system_t procfs = {
    .name = "proc",
    .mount = &procfs_mount,
};

static file_operations_t procfs_root_fops = {
    .open = &procfs_open,
    .close = &procfs_close,
    .readdir = &procfs_root_readdir,
};

static inode_operations_t procfs_root_iops = {
    .lookup = &procfs_root_lookup,
};

static inode_operations_t procfs_pid_iops = {
    .lookup = &procfs_pid_lookup,
    .readlink = &procfs_pid_readlink,
};

static file_operations_t procfs_pid_fops = {
    .open = &procfs_open,
    .close = &procfs_close,
    .readdir = &procfs_pid_readdir,
};

static file_operations_t procfs_fd_fops = {
    .open = &procfs_open,
    .close = &procfs_close,
    .readdir = &procfs_fd_readdir,
};

static inode_operations_t procfs_fd_iops = {
    .lookup = &procfs_fd_lookup,
    .readlink = &procfs_fd_readlink,
};

PROCFS_ENTRY(meminfo);
PROCFS_ENTRY(cmdline);
PROCFS_ENTRY(uptime);
PROCFS_ENTRY(syslog);

static generic_vfs_entry_t root_entries[] = {
    REG(meminfo, S_IFREG | S_IRUGO),
    REG(cmdline, S_IFREG | S_IRUGO),
    REG(uptime, S_IFREG | S_IRUGO),
    REG(syslog, S_IFREG | S_IRUGO),
};

PROCFS_ENTRY(comm);
PROCFS_ENTRY(status);
PROCFS_ENTRY(stack);
PROCFS_ENTRY(maps);
PROCFS_ENTRY(environ);

static generic_vfs_entry_t pid_entries[] = {
    DOT(.),
    DOT(..),
    REG(comm, S_IRUGO),
    REG(status, S_IRUGO),
    REG(stack, S_IRUGO),
    REG(maps, S_IRUGO),
    REG(environ, S_IRUGO),
    REG(cmdline, S_IRUGO),
    DIR(fd, S_IRUGO | S_IWUSR | S_IXUGO, &procfs_fd_iops, &procfs_fd_fops)
};

int procfs_init(void)
{
    return file_system_register(&procfs);
}

static int procfs_mount(super_block_t* sb, inode_t* inode, void*, int)
{
    static super_operations_t procfs_sb_ops;

    sb->ops = &procfs_sb_ops;
    sb->module = 0;

    inode->ino = GENERIC_VFS_ROOT_INO;
    inode->ops = &procfs_root_iops;
    inode->fs_data = NULL;
    inode->file_ops = &procfs_root_fops;
    inode->size = 0;
    inode->sb = sb;

    return 0;
}

static int procfs_root_lookup(inode_t*, const char* name, inode_t** result)
{
    int errno, pid = 0, pid_ino;
    inode_t* new_inode;
    generic_vfs_entry_t* entry = NULL;
    process_t* p;

    if (!strcmp(name, "self"))
    {
        pid = -1;
        pid_ino = SELF_INO;

        if (unlikely(errno = inode_alloc(&new_inode)))
        {
            log_error("cannot get inode, errno %d", errno);
            return errno;
        }

        new_inode->ops = &procfs_pid_iops;
        new_inode->file_ops = &procfs_pid_fops;
        new_inode->ino = SELF_INO;
        new_inode->mode = S_IFLNK | S_IRUGO | S_IXUGO;
        new_inode->fs_data = NULL;

        *result = new_inode;

        return 0;
    }
    else
    {
        pid = strtoi(name);
        pid_ino = PID_TO_INO(pid);
    }

    if (pid)
    {
        procfs_pid_data_t* data;
        if (process_find(pid, &p) && pid != -1)
        {
            return -ENOENT;
        }

        if (unlikely(errno = inode_alloc(&new_inode)))
        {
            return errno;
        }

        if (unlikely(!(data = alloc(typeof(*data)))))
        {
            inode_put(new_inode);
            return -ENOMEM;
        }

        list_init(&data->entries);

        new_inode->ops = &procfs_pid_iops;
        new_inode->file_ops = &procfs_pid_fops;
        new_inode->ino = pid_ino;
        new_inode->mode = S_IFDIR | S_IRUGO | S_IXUGO;
        new_inode->fs_data = data;

        p->procfs_inode = new_inode;

        *result = new_inode;
        log_debug(DEBUG_PROCFS, "finished succesfully for %s %O", name, *result);
        return 0;
    }

    entry = generic_vfs_find(name, root_entries, array_size(root_entries));

    if (!entry)
    {
        log_debug(DEBUG_PROCFS, "no such entry: %s", name);
        return -ENOENT;
    }

    if (unlikely(errno = inode_alloc(&new_inode)))
    {
        log_error("cannot get inode, errno %d", errno);
        return errno;
    }

    new_inode->ops = entry->iops;
    new_inode->file_ops = entry->fops;
    new_inode->ino = entry->ino;
    new_inode->mode = entry->mode;
    new_inode->fs_data = entry;

    *result = new_inode;

    log_debug(DEBUG_PROCFS, "finished succesfully %O", *result);

    return 0;
}

static int procfs_pid_readlink(inode_t* inode, char* buffer, size_t size)
{
    if (unlikely(inode->ino != (ino_t)SELF_INO))
    {
        return -EINVAL;
    }

    return snprintf(buffer, size, "%u", process_current->pid) + 1;
}

static int procfs_open(file_t*)
{
    return 0;
}

static int procfs_close(file_t*)
{
    return 0;
}

static int procfs_root_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    int i = 0;
    char type;
    size_t len;
    generic_vfs_entry_t* entry;
    process_t* p;
    char namebuf[12];

    log_debug(DEBUG_PROCFS, "inode=%O", file->dentry->inode);

    if (file->offset == READDIR_END_OFFSET)
    {
        return 0;
    }

    if (dirent_add(buf, ".", 4, 0, DT_DIR))
    {
        log_debug(DEBUG_PROCFS, "adding .");
        goto finish;
    }
    ++i;

    if (dirent_add(buf, "..", 4, 0, DT_DIR))
    {
        log_debug(DEBUG_PROCFS, "adding ..");
        goto finish;
    }
    ++i;

    for_each_process(p)
    {
        len = snprintf(namebuf, sizeof(namebuf), "%u", p->pid);
        log_debug(DEBUG_PROCFS, "adding %s", namebuf);
        if (dirent_add(buf, namebuf, len, PID_TO_INO(p->pid), DT_DIR))
        {
            goto finish;
        }
        ++i;
    }

    if (dirent_add(buf, "self", 4, SELF_INO, DT_LNK))
    {
        log_debug(DEBUG_PROCFS, "adding self");
        goto finish;
    }
    ++i;

    for (size_t j = 0; j < array_size(root_entries); ++j, ++i)
    {
        type = DT_DIR;
        entry = &root_entries[j];
        len = strlen(entry->name);

        type = S_ISDIR(entry->mode) ? DT_DIR : DT_REG;

        log_debug(DEBUG_PROCFS, "adding %s", entry->name);
        if (dirent_add(buf, entry->name, len, 0, type))
        {
            break;
        }
    }

    file->offset = READDIR_END_OFFSET;

finish:
    return i;
}

static int procfs_pid_lookup(inode_t* dir, const char* name, inode_t** result)
{
    int errno;
    process_t* p;
    inode_t* new_inode;
    generic_vfs_entry_t* entry = NULL;
    procfs_pid_data_t* data;

    if (unlikely(!(p = procfs_process_from_inode(dir))))
    {
        return -ESRCH;
    }

    entry = generic_vfs_find(name, pid_entries, array_size(pid_entries));

    if (!entry)
    {
        log_debug(DEBUG_PROCFS, "no such entry: %s", name);
        return -ENOENT;
    }

    if (unlikely(errno = inode_alloc(&new_inode)))
    {
        log_error("cannot get inode, errno %d", errno);
        return errno;
    }

    data = p->procfs_inode->fs_data;

    new_inode->ops = entry->iops;
    new_inode->file_ops = entry->fops;
    new_inode->ino = dir->ino | entry->ino;
    new_inode->mode = entry->mode;
    new_inode->fs_data = entry;

    list_add(&new_inode->list, &data->entries);

    *result = new_inode;

    log_debug(DEBUG_PROCFS, "finished succesfully %O", *result);

    return 0;
}

static int procfs_pid_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    int i;
    char type;
    size_t len;
    generic_vfs_entry_t* entry;

    log_debug(DEBUG_PROCFS, "inode=%O", file->dentry->inode);

    if (file->offset == READDIR_END_OFFSET)
    {
        return 0;
    }

    for (i = 0; i < (int)array_size(pid_entries); ++i)
    {
        type = DT_DIR;
        entry = &pid_entries[i];
        len = strlen(entry->name);

        type = S_ISDIR(entry->mode) ? DT_DIR : DT_REG;

        log_debug(DEBUG_PROCFS, "adding %s", entry->name);
        if (dirent_add(buf, entry->name, len, 0, type))
        {
            break;
        }
    }

    file->offset = READDIR_END_OFFSET;

    return i;
}

static int comm_show(seq_file_t* s)
{
    process_t* p = procfs_process_from_seqfile(s);
    seq_puts(s, p->name);
    return 0;
}

static int procfs_fd_lookup(inode_t* dir, const char* name, inode_t** result)
{
    int errno;
    process_t* p;
    inode_t* new_inode;
    int fd = strtoi(name);
    procfs_pid_data_t* data;

    if (unlikely(!(p = procfs_process_from_inode(dir))))
    {
        return -ESRCH;
    }

    if (unlikely(fd < 0 || fd >= PROCESS_FILES))
    {
        return -ESRCH;
    }

    file_t* file = p->files->files[fd];

    if (unlikely(!file))
    {
        return -ESRCH;
    }

    if (unlikely(errno = inode_alloc(&new_inode)))
    {
        log_error("cannot get inode, errno %d", errno);
        return errno;
    }

    data = p->procfs_inode->fs_data;

    new_inode->ops = &procfs_fd_iops;
    new_inode->file_ops = &procfs_fd_fops;
    new_inode->ino = PID_TO_INO(p->pid) | fd << 24;
    new_inode->mode = 0644 | S_IFLNK;

    list_add(&new_inode->list, &data->entries);

    *result = new_inode;

    return 0;
}

static int procfs_fd_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    int count = 0;
    process_t* p;
    char namebuf[12];

    if (file->offset == READDIR_END_OFFSET)
    {
        return 0;
    }

    if (unlikely(!(p = procfs_process_from_inode(file->dentry->inode))))
    {
        return -ESRCH;
    }

    for (int i = 0; i < PROCESS_FILES; ++i)
    {
        if (!p->files->files[i])
        {
            continue;
        }

        snprintf(namebuf, sizeof(namebuf), "%u", i);
        count++;

        if (dirent_add(buf, namebuf, strlen(namebuf), PID_TO_INO(p->pid) | FD_TO_INO(i), DT_LNK))
        {
            return count;
        }
    }

    file->offset = READDIR_END_OFFSET;

    return count;
}

static int procfs_fd_readlink(inode_t* inode, char* buffer, size_t size)
{
    int errno;
    process_t* p;
    int fd = INO_TO_FD(inode->ino);

    if (unlikely(!(p = procfs_process_from_inode(inode))))
    {
        return -ESRCH;
    }

    if (unlikely(fd < 0 || fd >= PROCESS_FILES))
    {
        return -ESRCH;
    }

    file_t* file = p->files->files[fd];

    if (unlikely(!file))
    {
        return -ESRCH;
    }

    dentry_t* dentry = file->dentry;

    if (unlikely(!dentry))
    {
        return -ENOENT;
    }

    if (unlikely(errno = path_construct(dentry, buffer, size)))
    {
        return errno;
    }

    return strlen(buffer);
}

void procfs_unlink(inode_t* inode)
{
    if (inode->dentry)
    {
        dentry_delete(inode->dentry);
    }
    inode_put(inode);
}

// FIXME: some locking is needed to handle parallel
// removal and reading from different process
void procfs_cleanup(process_t* p)
{
    inode_t* inode;
    procfs_pid_data_t* data;

    if (!p->procfs_inode)
    {
        return;
    }

    data = p->procfs_inode->fs_data;

    // FIXME:  it should do backwards iteration
    list_for_each_entry_safe(inode, &data->entries, list)
    {
        procfs_unlink(inode);
    }

    procfs_unlink(p->procfs_inode);

    delete(data);
}

static int status_show(seq_file_t* s)
{
    vm_area_t* vma;
    size_t code_size, stack_size, data_size;
    process_t* p = procfs_process_from_seqfile(s);
    const char* state;

    if (unlikely(!p))
    {
        return -ESRCH;
    }

    switch (p->stat)
    {
        case PROCESS_RUNNING: state = "running"; break;
        case PROCESS_WAITING: state = "waiting"; break;
        case PROCESS_STOPPED: state = "stopped"; break;
        case PROCESS_ZOMBIE: state = "zombie"; break;
        default: state = "unknown";
    }

    seq_printf(s, "Name:    %s\n", p->name);
    seq_printf(s, "State:   %c (%s)\n", process_state_char(p->stat), state);
    seq_printf(s, "Pid:     %u\n", p->pid);
    seq_printf(s, "Ppid:    %u\n", p->ppid);

    if (p->type == USER_PROCESS)
    {
        vma = process_stack_vm_area(p);
        stack_size = vma ? vma->end - vma->start : 0;

        vma = process_brk_vm_area(p);
        data_size = vma ? vma->end - vma->start : 0;

        vma = process_code_vm_area(p);
        code_size = vma ? vma->end - vma->start : 0;

        seq_printf(s, "VmSize:  %u kB\n", (code_size + stack_size + data_size) / KiB);
        seq_printf(s, "VmExe:   %u kB\n", code_size / KiB);
        seq_printf(s, "VmData:  %u kB\n", data_size / KiB);
        seq_printf(s, "VmStack: %u kB\n", stack_size / KiB);
    }

    seq_printf(s, "SigHan: %08x\n", p->signals->trapped);
    seq_printf(s, "ctxt_switches: %u\n", p->context_switches);

    return 0;
}

static int stack_show(seq_file_t* s)
{
    backtrace_process(procfs_process_from_seqfile(s), (void*)seq_printf, s);
    return 0;
}

static int meminfo_show(seq_file_t* s)
{
    seq_printf(s, "MemTotal: %u kB\n", usable_ram / KiB);
    return 0;
}

struct mapped_addr
{
    page_t* page;
    void* vaddr;
};

typedef struct mapped_addr mapped_addr_t;

static mapped_addr_t mapped_address_get(process_t* p, uintptr_t addr)
{
    page_t* page;
    uintptr_t paddr = vm_paddr(addr, p->mm->pgd);
    mapped_addr_t mapped_addr = {};

    if (unlikely(!paddr))
    {
        return mapped_addr;
    }

    page = page(paddr);

    page_kernel_map(page, kernel_identity_pgprot(0));

    mapped_addr.page = page;
    mapped_addr.vaddr = page_virt_ptr(page) + (addr & PAGE_MASK);

    return mapped_addr;
}

static void mapped_address_put(mapped_addr_t* addr)
{
    page_kernel_unmap(addr->page);
}

static void memory_to_seq_file(seq_file_t* s, const char* data, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        seq_putc(s, data[i]);
    }
}

static int cmdline_show(seq_file_t* s)
{
    process_t* p = procfs_process_from_seqfile(s);

    if (p)
    {
        mapped_addr_t addr = mapped_address_get(p, p->mm->args_start);

        const char* cmdline = addr.vaddr;

        if (unlikely(!cmdline))
        {
            return -ENOMEM;
        }

        memory_to_seq_file(s, cmdline, p->mm->args_end - p->mm->args_start);

        mapped_address_put(&addr);

        return 0;
    }

    seq_puts(s, cmdline);
    seq_putc(s, '\n');
    return 0;
}

static int uptime_show(seq_file_t* s)
{
    timeval_t ts;
    timestamp_get(&ts);
    seq_printf(s, "%u.%u\n", ts.tv_sec, ts.tv_usec);
    return 0;
}

static int environ_show(seq_file_t* s)
{
    process_t* p = procfs_process_from_seqfile(s);

    if (unlikely(!p))
    {
        return -ESRCH;
    }

    mapped_addr_t addr = mapped_address_get(p, p->mm->env_start);

    const char* env = addr.vaddr;

    if (unlikely(!env))
    {
        return -ENOMEM;
    }

    memory_to_seq_file(s, env, p->mm->env_end - p->mm->env_start);

    mapped_address_put(&addr);

    return 0;
}
