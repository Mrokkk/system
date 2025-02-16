#include <stdarg.h>

#include <kernel/fs.h>
#include <kernel/cpu.h>
#include <kernel/irq.h>
#include <kernel/time.h>
#include <kernel/ksyms.h>
#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/process.h>
#include <kernel/termios.h>
#include <kernel/api/ioctl.h>
#include <kernel/api/unistd.h>

#include <arch/rtc.h>
#include <arch/register.h>

#define DEREFERENCE 0x1

KERNEL_MODULE(debugmon);
module_init(debugmon_init);
module_exit(debugmon_deinit);

static file_t* file;
static char* printf_buf;

static int initialize(void);

static char* mm_print(const struct mm* mm, char* str, const char* end)
{
    str += snprintf(
        str,
        end - str,
        "mm{\n"
        "\t\taddr = %p,\n"
        "\t\tstack_start =  %p,\n"
        "\t\tstack_end =    %p,\n"
        "\t\targs_start =   %p,\n"
        "\t\targs_end =     %p,\n"
        "\t\tenv_start =    %p,\n"
        "\t\tenv_end =      %p,\n"
        "\t\tbrk =          %p,\n"
        "\t\tpgd =          %p,\n"
        "\t}",
        mm,
        ptr(mm->stack_start), ptr(mm->stack_end),
        ptr(mm->args_start), ptr(mm->args_end),
        ptr(mm->env_start), ptr(mm->env_end),
        ptr(mm->brk),
        mm->pgd);
    return str;
}

static char* fs_print(const void* data, char* str, const char* end)
{
    const struct fs* fs = data;
    str += snprintf(
        str,
        end - str,
        "fs{\n"
        "\t\taddr = %p,\n"
        "\t\trefcount = %u,\n"
        "\t\tcwd = %p\n"
        "\t}", fs, fs->refcount, fs->cwd);
    return str;
}

static char* process_print(const process_t* p, char* str, const char* end)
{
    str += snprintf(
        str,
        end - str,
        "process{\n"
        "\taddr = %p,\n"
        "\tname = \"%s\"\n"
        "\tpid = %u\n"
        "\tppid = %u\n"
        "\tstat = %c,\n"
        "\ttype = %u,\n"
        "\tcontext_switches = %u,\n"
        "\tforks = %u,\n"
        "\t\tkernel_stack = %p,\n"
        "\tmm = ",
        p,
        p->name,
        p->pid,
        p->ppid,
        process_state_char(p->stat),
        p->type,
        p->context_switches,
        p->forks,
        p->kernel_stack);

    str = mm_print(p->mm, str, end);
    str += snprintf(str, end - str, ",\n\tfs = ");
    str = fs_print(p->fs, str, end);
    str += snprintf(str, end - str, "\n}");

    return str;
}

static inline void read_line(char* line)
{
    do_read(file, 0, line, 32);
    char* newline = __builtin_strrchr(line, '\n');
    *newline = 0;
}

static int printf(const char* fmt, ...)
{
    int printed;
    va_list args;

    va_start(args, fmt);
    printed = vsnprintf(printf_buf, PAGE_SIZE, fmt, args);
    va_end(args);

    do_write(file, 0, printf_buf, strlen(printf_buf));

    return printed;
}

static int c_fs()
{
    file_systems_print();
    return 0;
}

static int c_ps()
{
    process_t* proc;
    for_each_process(proc)
    {
        printf("pid=%d name=%s stat=%c\n",
            proc->pid,
            proc->name,
            process_state_char(proc->stat));
    }
    return 0;
}

#define print_string(data) \
    printf(#data"=%s\n", data);

#define print_ul(data) \
    printf(#data"=%u\n", data);

static int c_cpu()
{
    print_string(cpu_info.vendor);
    print_string(cpu_info.producer);
    print_string(cpu_info.name);
    print_ul(cpu_info.family);
    print_ul(cpu_info.model);
    print_ul(cpu_info.stepping);
    print_ul(cpu_info.cacheline_size);
    print_ul(cpu_info.cache_size);
    return 0;
}

static int c_rtc()
{
    rtc_print();
    return 0;
}

static int c_crash()
{
    ASSERT(cr3_get() == phys_addr(process_current->mm->pgd));
    uint32_t* a = ptr(0);
    DIAG_IGNORE("-Wanalyzer-null-dereference");
    *a = 234;
    DIAG_RESTORE();
    return 0;
}

static int c_kstat()
{
    timeval_t ts;
    timestamp_get(&ts);
    int uptime = jiffies / HZ;
    print_ul(jiffies);
    print_ul(ts.tv_sec);
    print_ul(ts.tv_usec);
    print_ul(uptime);
    print_ul(context_switches);
    print_ul(total_forks);
    return 0;
}

static int c_running()
{
    process_t* proc;
    list_for_each_entry(proc, &running, running)
    {
        printf("pid=%d name=%s stat=%d\n",
            proc->pid,
            proc->name,
            proc->stat);
    }
    return 0;
}

#define COMMAND(name) {#name, c_##name}

static struct command {
    char *name;
    int (*function)();
} commands[] = {
    COMMAND(fs),
    COMMAND(ps),
    COMMAND(cpu),
    COMMAND(rtc),
    COMMAND(crash),
    COMMAND(kstat),
    COMMAND(running),
    {0, 0}
};

static inline int address_is_mapped(uintptr_t addr)
{
    return vm_paddr(addr, process_current->mm->pgd);
}

process_t* process_get(int pid)
{
    process_t* p;
    for_each_process(p)
    {
        if (p->pid == pid)
        {
            return p;
        }
    }
    return NULL;
}

static int initialize(void)
{
    int errno;
    page_t* page = page_alloc(1, 0);

    if (unlikely(!page))
    {
        log_error("cannot allocate page for buffer");
        return -1;
    }

    printf_buf = page_virt_ptr(page);

    do_chroot(NULL);
    do_chroot("/root");
    do_chdir("/");

    errno = do_open(&file, "/dev/ttyS0", O_RDWR, 0);

    if (unlikely(errno))
    {
        log_error("cannot open: %d", errno);
        return -1;
    }

    termios_t termios;
    do_ioctl(file, TCGETA, &termios);

    termios.c_lflag &= ~ECHO;

    do_ioctl(file, TCSETA, &termios);

    return 0;
}

static void line_handle(const char* line)
{
    int i, pid, flags = 0;
    ksym_t* sym;
    process_t* p;
    const char* line_it = line;
    char* str;
    uint32_t* ptr;
    struct command* com = commands;

    if (line[0] == 0)
    {
        return;
    }
    else if (line[0] == '*')
    {
        ++line_it;
        flags = DEREFERENCE;
    }
    else if (line[0] == '$')
    {
        ++line_it;
        pid = *line_it - '0'; // FIXME: add proper strtol/atoi/strtonum
        process_find(pid, &p);
        if (!p)
        {
            printf("%s = <null>\n", line);
            return;
        }

        str = process_print(p, printf_buf, printf_buf + PAGE_SIZE);
        do_write(file, 0, printf_buf, str - printf_buf);
        printf("\n");
        return;
    }

    for (i = 0; com[i].name; i++)
    {
        if (!strcmp(com[i].name, line_it))
        {
            com[i].function();
            return;
        }
    }

    if (!(sym = ksym_find_by_name(line_it)))
    {
        printf("No such command: %s\n", line);
        return;
    }

    ptr = ptr(sym->address);
    if (flags & DEREFERENCE)
    {
        if (address_is_mapped(*ptr))
        {
            printf("%s = %O\n", line, *ptr);
        }
        else
        {
            printf("cannot read %x, address not mapped\n", *ptr);
        }
    }
    else
    {
        printf("%s = %O\n", line, ptr);
    }
}

static void NORETURN(debug_monitor())
{
    char line[32];

    if (initialize())
    {
        flags_t flags;
        log_warning("failed to start debugmon");
        irq_save(flags);
        process_wait2(flags);
        ASSERT_NOT_REACHED();
    }

    while (1)
    {
        printf("monitor> ");
        read_line(line);
        line_handle(line);
    }
}

static int debugmon_init(void)
{
    process_spawn("debugmon", &debug_monitor, NULL, SPAWN_KERNEL);
    return 0;
}

static int debugmon_deinit(void)
{
    return 0;
}
