#include <stdarg.h>

#include <kernel/fs.h>
#include <kernel/cpu.h>
#include <kernel/irq.h>
#include <kernel/page.h>
#include <kernel/time.h>
#include <kernel/ksyms.h>
#include <kernel/device.h>
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

static int fd;
static char* printf_buf;

static int initialize(void);

static inline void read_line(char* line)
{
    int size = read(fd, line, 32);
    line[size - 1] = 0;
}

static int printf(const char* fmt, ...)
{
    int printed;
    va_list args;

    va_start(args, fmt);
    printed = vsprintf(printf_buf, fmt, args);
    va_end(args);

    write(fd, printf_buf, strlen(printf_buf));

    return printed;
}

static int c_fs()
{
    file_systems_print();
    return 0;
}

static int c_ps()
{
    struct process* proc;
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
    print_ul(cpu_info.mhz);
    print_ul(cpu_info.cacheline_size);
    print_ul(cpu_info.cache_size);
    print_ul(cpu_info.bogomips);
    print_string(cpu_info.features_string);
    return 0;
}

static int c_rtc()
{
    rtc_print();
    return 0;
}

static int c_page()
{
    page_stats_print();
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
    struct process* proc;
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
    COMMAND(page),
    COMMAND(crash),
    COMMAND(kstat),
    COMMAND(running),
    {0, 0}
};

static inline int address_is_mapped(uint32_t addr)
{
    pgt_t* pgt;
    pgd_t* pgd = process_current->mm->pgd;
    uint32_t pde_index = pde_index(addr);
    uint32_t pte_index = pte_index(addr);

    if (!pgd[pde_index])
    {
        return 0;
    }

    pgt = virt_ptr(pgd[pde_index] & PAGE_ADDRESS);

    return pgt[pte_index];
}

struct process* process_get(int pid)
{
    struct process* p;
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
    page_t* page = page_alloc(1, PAGE_ALLOC_DISCONT);

    if (unlikely(!page))
    {
        log_error("cannot allocate page for buffer");
        return -1;
    }

    printf_buf = page_virt_ptr(page);

    fd = open("/dev/ttyS0", O_RDWR, 0);

    if (fd < 0)
    {
        log_error("cannot open: %d", fd);
        return -1;
    }

    termios_t termios;
    ioctl(fd, TCGETA, &termios);

    termios.c_lflag &= ~ECHO;

    ioctl(fd, TCSETA, &termios);

    return 0;
}

static void line_handle(const char* line)
{
    int i, pid, flags = 0;
    ksym_t* sym;
    struct process* p;
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

        str = process_print(p, printf_buf);
        write(fd, printf_buf, str - printf_buf);
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
