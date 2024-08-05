#define log_fmt(fmt) "procfs: " fmt
#include "procfs.h"
#include <kernel/fs.h>
#include <kernel/init.h>
#include <kernel/time.h>
#include <kernel/memory.h>
#include <kernel/minmax.h>
#include <kernel/process.h>
#include <kernel/seq_file.h>
#include <kernel/vm_print.h>

#define DEBUG_PROCFS 0

static int procfs_mount(super_block_t* sb, inode_t* inode, void*, int);
static int procfs_open(file_t* file);
static int procfs_close(file_t* file);

static int procfs_root_lookup(inode_t* dir, const char* name, inode_t** result);
static int procfs_root_readdir(file_t* file, void* buf, direntadd_t dirent_add);

static int procfs_pid_lookup(inode_t* dir, const char* name, inode_t** result);
static int procfs_pid_readdir(file_t* file, void* buf, direntadd_t dirent_add);

static int comm_show(seq_file_t* s);
static int status_show(seq_file_t* s);
static int stack_show(seq_file_t* s);
static int meminfo_show(seq_file_t* s);
static int cmdline_show(seq_file_t* s);
static int uptime_show(seq_file_t* s);
static int environ_show(seq_file_t* s);
int syslog_show(seq_file_t* s);
int maps_show(seq_file_t* s);

#define NODE(NAME, MODE, IOPS, FOPS) \
    { \
        .name = NAME, \
        .len = sizeof(NAME), \
        .mode = MODE, \
        .ino = 2 + __COUNTER__, \
        .iops = IOPS, \
        .fops = FOPS, \
    }

#define DIR(name, mode, iops, fops) \
    NODE(name, S_IFDIR | mode, iops, fops)

#define REG(name, mode) \
    NODE(#name, S_IFREG | mode, &name##_iops, &name##_fops)

#define LNK(name, mode) \
    NODE(#name, S_IFLNK | mode, NULL, NULL)

#define DOT(name) \
    DIR(name, S_IFDIR | S_IRUGO | S_IWUSR, NULL, NULL)

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

static super_operations_t procfs_sb_ops;

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
};

static file_operations_t procfs_pid_fops = {
    .open = &procfs_open,
    .close = &procfs_close,
    .readdir = &procfs_pid_readdir,
};

PROCFS_ENTRY(meminfo);
PROCFS_ENTRY(cmdline);
PROCFS_ENTRY(uptime);
PROCFS_ENTRY(syslog);

static procfs_entry_t root_entries[] = {
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

static procfs_entry_t pid_entries[] = {
    DOT("."),
    DOT(".."),
    REG(comm, S_IRUGO),
    REG(status, S_IRUGO),
    REG(stack, S_IRUGO),
    REG(maps, S_IRUGO),
    REG(environ, S_IRUGO),
    REG(cmdline, S_IRUGO),
};

static inline procfs_entry_t* procfs_find(const char* name, procfs_entry_t* dir, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (!strcmp(name, dir[i].name))
        {
            log_debug(DEBUG_PROCFS, "found node: %S", name);
            return &dir[i];
        }
    }

    return NULL;
}

int procfs_init(void)
{
    return file_system_register(&procfs);
}

static int procfs_mount(super_block_t* sb, inode_t* inode, void*, int)
{
    sb->ops = &procfs_sb_ops;
    sb->module = 0;

    inode->ino = DEVFS_ROOT_INO;
    inode->ops = &procfs_root_iops;
    inode->fs_data = NULL;
    inode->file_ops = &procfs_root_fops;
    inode->size = 0;
    inode->sb = sb;

    return 0;
}

static int procfs_root_lookup(inode_t* dir, const char* name, inode_t** result)
{
    int errno, pid = 0, pid_ino;
    inode_t* new_inode;
    procfs_entry_t* entry = NULL;
    process_t* p;

    if (!strcmp(name, "self"))
    {
        pid = -1;
        pid_ino = SELF_INO;
    }
    else
    {
        pid = strtoi(name);
        pid_ino = PID_TO_INO(pid);
    }

    if (pid)
    {
        if (process_find(pid, &p) && pid != -1)
        {
            return -ENOENT;
        }

        if (unlikely(errno = inode_get(&new_inode)))
        {
            log_error("cannot get inode, errno %d", errno);
            return errno;
        }

        new_inode->ops = &procfs_pid_iops;
        new_inode->file_ops = &procfs_pid_fops;
        new_inode->ino = pid_ino;
        new_inode->sb = dir->sb;
        new_inode->mode = S_IFDIR | S_IRUGO | S_IXUGO;
        new_inode->fs_data = NULL;

        *result = new_inode;
        log_debug(DEBUG_PROCFS, "finished succesfully for %s %O", name, *result);
        return 0;
    }

    entry = procfs_find(name, root_entries, array_size(root_entries));

    if (!entry)
    {
        log_debug(DEBUG_PROCFS, "no such entry: %s", name);
        return -ENOENT;
    }

    if (unlikely(errno = inode_get(&new_inode)))
    {
        log_error("cannot get inode, errno %d", errno);
        return errno;
    }

    new_inode->ops = entry->iops;
    new_inode->file_ops = entry->fops;
    new_inode->ino = entry->ino;
    new_inode->sb = dir->sb;
    new_inode->mode = entry->mode;
    new_inode->fs_data = entry;

    *result = new_inode;

    log_debug(DEBUG_PROCFS, "finished succesfully %O", *result);

    return 0;
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
    procfs_entry_t* entry;
    process_t* p;
    char namebuf[12];

    log_debug(DEBUG_PROCFS, "inode=%O", file->inode);

    ++i;
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

    for_each_process(p)
    {
        len = snprintf(namebuf, sizeof(namebuf), "%u", p->pid);
        log_debug(DEBUG_PROCFS, "adding %s", namebuf);
        ++i;
        if (dirent_add(buf, namebuf, len, 0, DT_DIR))
        {
            goto finish;
        }
    }

    ++i;
    if (dirent_add(buf, "self", 4, 0, DT_DIR))
    {
        log_debug(DEBUG_PROCFS, "adding self");
        goto finish;
    }

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

finish:
    return i;
}

static int procfs_pid_lookup(inode_t* dir, const char* name, inode_t** result)
{
    int errno;
    inode_t* new_inode;
    procfs_entry_t* entry = NULL;

    entry = procfs_find(name, pid_entries, array_size(pid_entries));

    if (!entry)
    {
        log_debug(DEBUG_PROCFS, "no such entry: %s", name);
        return -ENOENT;
    }

    if (unlikely(errno = inode_get(&new_inode)))
    {
        log_error("cannot get inode, errno %d", errno);
        return errno;
    }

    new_inode->ops = entry->iops;
    new_inode->file_ops = entry->fops;
    new_inode->ino = dir->ino | entry->ino;
    new_inode->sb = dir->sb;
    new_inode->mode = entry->mode;
    new_inode->fs_data = entry;

    *result = new_inode;

    log_debug(DEBUG_PROCFS, "finished succesfully %O", *result);

    return 0;
}

static int procfs_pid_readdir(file_t* file, void* buf, direntadd_t dirent_add)
{
    int i;
    char type;
    size_t len;
    procfs_entry_t* entry;

    log_debug(DEBUG_PROCFS, "inode=%O", file->inode);

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

    return i;
}

static int comm_show(seq_file_t* s)
{
    process_t* p = procfs_process_from_seqfile(s);
    seq_puts(s, p->name);
    return 0;
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

static const char* kernel_address_get(process_t* p, uintptr_t addr)
{
    uintptr_t paddr = vm_paddr(addr, p->mm->pgd);

    if (!paddr)
    {
        return NULL;
    }

    return virt_ptr(paddr);
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
        const char* env = kernel_address_get(p, p->mm->args_start);

        if (unlikely(!env))
        {
            return -ENOMEM;
        }

        memory_to_seq_file(s, env, p->mm->args_end - p->mm->args_start);

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

    const char* env = kernel_address_get(p, p->mm->env_start);

    if (unlikely(!env))
    {
        return -ENOMEM;
    }

    memory_to_seq_file(s, env, p->mm->env_end - p->mm->env_start);

    return 0;
}
