#define log_fmt(fmt) "init: " fmt
#include <stdarg.h>
#include <kernel/cpu.h>
#include <kernel/tty.h>
#include <kernel/init.h>
#include <kernel/time.h>
#include <kernel/devfs.h>
#include <kernel/ksyms.h>
#include <kernel/timer.h>
#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/module.h>
#include <kernel/procfs.h>
#include <kernel/process.h>
#include <kernel/sections.h>
#include <kernel/backtrace.h>
#include <kernel/api/unistd.h>

#include <arch/idle.h>
#include <arch/earlycon.h>
#include <arch/multiboot.h>

static void init(const char* cmdline);

unsigned init_in_progress = INIT_IN_PROGRESS;
char cmdline[CMDLINE_SIZE];
char* bootloader_name;
static param_t* params;

void breakpoint(void)
{
}

NOINLINE static void NORETURN(spawn_init_and_go_idle(void))
{
    sti();

    process_spawn("init", &init, cmdline, SPAWN_KERNEL);

    // Remove process from the running queue and reschedule
    process_stop(process_current);
    scheduler();

    idle();

    ASSERT_NOT_REACHED();
}

UNMAP_AFTER_INIT static param_t* params_read(char buffer[CMDLINE_SIZE], param_t output[CMDLINE_PARAMS_COUNT])
{
    char* save_ptr;
    char* tmp = buffer;
    char* previous_token = NULL;
    char* new_token = NULL;
    size_t count = 0;

    do
    {
        new_token = strtok_r(tmp, "= ", &save_ptr);
        if (previous_token)
        {
            bool value_param      = cmdline[new_token - buffer - 1] == '=';
            output[count].name    = previous_token;
            output[count++].value = value_param ? new_token : NULL;
            previous_token        = value_param ? NULL : new_token;
        }
        else
        {
            previous_token = new_token;
        }
        tmp = NULL;
    }
    while (new_token && count < CMDLINE_PARAMS_COUNT - 1);

    return output;
}

param_t* param_get(param_name_t name)
{
    for (param_t* p = params; p->name; ++p)
    {
        if (!strcmp(p->name, name.name))
        {
            return p;
        }
    }
    return NULL;
}

const char* param_value_get(param_name_t name)
{
    param_t* param = param_get(name);
    return param ? param->value : NULL;
}

const char* param_value_or_get(param_name_t name, const char* def)
{
    param_t* param = param_get(name);
    return param ? param->value : def;
}

bool param_bool_get(param_name_t name)
{
    return !!param_get(name);
}

void param_call_if_set(param_name_t name, int (*action)())
{
    param_t* p;
    int error;
    if ((p = param_get(name)))
    {
        if ((error = action(p)))
        {
            log_warning("action associated with %s failed with %d", name, error);
        }
    }
}

UNMAP_AFTER_INIT static void boot_params_print(void)
{
    log_notice("bootloader: %s", bootloader_name);
    log_notice("cmdline: %s", cmdline);
}

UNMAP_AFTER_INIT static void memory_print(void)
{
    uint32_t ram_hi = addr(full_ram >> 32);
    uint32_t ram_low = addr(full_ram);
    uint32_t mib = 4096 * ram_hi + ram_low / MiB;

    log_notice("RAM: %u MiB", mib);

    if (full_ram != (uint64_t)usable_ram)
    {
        log_continue("; usable %u MiB (%u B)", usable_ram / MiB, usable_ram);
    }

    memory_areas_print();
    sections_print();
}

UNMAP_AFTER_INIT void NORETURN(kmain(void* data, ...))
{
    va_list args;
    timeval_t ts;
    const char* temp_cmdline;
    char buffer[CMDLINE_SIZE];
    param_t parameters[CMDLINE_PARAMS_COUNT];

    bss_zero();

    va_start(args, data);
    temp_cmdline = multiboot_read(args);
    va_end(args);

    strcpy(cmdline, temp_cmdline);
    strcpy(buffer, temp_cmdline);

    boot_params_print();

    params = params_read(buffer, parameters);

    arch_setup();

    memory_print();
    paging_init();
    ksyms_load(ksyms_start, ksyms_end);
    fmalloc_init();
    slab_allocator_init();
    devfs_init();
    procfs_init();
    processes_init();

    arch_late_setup();

    modules_init();

    ASSERT(init_in_progress == INIT_IN_PROGRESS);
    init_in_progress = 0;

    timestamp_get(&ts);

    if (ts.tv_sec || ts.tv_usec)
    {
        log_notice("boot finished in %u.%06u s", ts.tv_sec, ts.tv_usec);
    }

    spawn_init_and_go_idle();

    ASSERT_NOT_REACHED();
}

UNMAP_AFTER_INIT static int root_mount(void)
{
    int errno;
    const char* sources[] = {"/dev/hdc", "/dev/sda0", "/dev/hda0", "/dev/img0"};
    const char* fs_types[] = {"iso9660", "ext2"};

    for (size_t i = 0; i < array_size(sources); ++i)
    {
        for (size_t j = 0; j < array_size(fs_types); ++j)
        {
            log_info("mounting root as %s on %s", fs_types[j], sources[i]);
            if ((errno = do_mount(sources[i], "/root", fs_types[j], 0)))
            {
                log_info("mounting %s on %s failed with %d", fs_types[j], sources[i], errno);
            }
            else
            {
                return 0;
            }
        }
    }

    return errno;
}

#define MUST_SUCCEED(title, fn, ...) \
    do \
    { \
        int errno = fn(__VA_ARGS__); \
        panic_if(errno, title ": failed with %d", errno); \
    } \
    while (0)

#define MAY_FAIL(title, fn, ...) \
    do \
    { \
        int errno = fn(__VA_ARGS__); \
        if (unlikely(errno)) \
        { \
            log_warning(title ": failed with %d", errno); \
        } \
    } \
    while (0)

UNMAP_AFTER_INIT static void fake_root_prepare(void)
{
    MUST_SUCCEED("mounting fake root",  do_mount, "none", "/", "ramfs", 0);
    MUST_SUCCEED("chroot to fake root", do_chroot, NULL);
    MUST_SUCCEED("chdir to fake root",  do_chdir, "/");
    MUST_SUCCEED("creating /root",      do_mkdir, "/root", 0555);
    MUST_SUCCEED("creating fake /dev",  do_mkdir, "/dev", 0555);
    MUST_SUCCEED("mounting fake /dev",  do_mount, "none", "/dev", "devfs", 0);
}

UNMAP_AFTER_INIT static void rootfs_prepare(void)
{
    fake_root_prepare();

    MUST_SUCCEED("mounting real root",  root_mount);
    MUST_SUCCEED("chroot to real root", do_chroot, "/root");
    MUST_SUCCEED("chdir to real root",  do_chdir, "/");
    MUST_SUCCEED("mounting /dev",       do_mount, "none", "/dev", "devfs", 0);
    MUST_SUCCEED("mounting /proc",      do_mount, "none", "/proc", "proc", 0);
    MAY_FAIL("mounting /tmp",           do_mount, "none", "/tmp", "ramfs", 0);
}

UNMAP_AFTER_INIT static void syslog_configure(void)
{
    int errno;
    tty_t* tty;
    file_t* file;
    const char* device = param_value_or_get(KERNEL_PARAM("syslog"), "none");

    log_notice("syslog output: %s", device);

    if (!strcmp(device, "none"))
    {
        return;
    }

    if (unlikely(errno = do_open(&file, device, O_RDWR, 0)))
    {
        log_error("failed to open syslog output device: %s", device);
        return;
    }

    if (unlikely(!(tty = tty_from_file(file))))
    {
        log_warning("%s: not a tty device", device);
        return;
    }

    printk_register(tty);
}

UNMAP_AFTER_INIT static void read_some_data(void)
{
    int errno;
    scoped_file_t* file = NULL;
    char* buf = single_page();
    const char* dev = "/dev/sda";

    if (unlikely(!buf))
    {
        return;
    }

    memset(buf, 0, PAGE_SIZE);

    if ((errno = do_open(&file, dev, O_RDONLY, 0)))
    {
        if ((errno = do_open(&file, dev = "/dev/hda", O_RDONLY, 0)))
        {
            log_warning("cannot open any disk");
        }
    }

    if (!errno)
    {
        log_info("reading data from %s", dev);
        do_read(file, 0, buf, PAGE_SIZE);
        memory_dump(KERN_INFO, buf, 5);
        memory_dump(KERN_INFO, buf + 508, 1);
    }

    page_free(buf);
}

static void unmap_sections(void)
{
    for (section_t* section = sections; section->name; ++section)
    {
        if (!(section->flags & SECTION_UNMAP_AFTER_INIT))
        {
            continue;
        }

        section_free(section);
    }
}

static void timer_callback(ktimer_t* timer)
{
    log_info("timer %u callback", timer->id);
}

static void NORETURN(init(const char* cmdline))
{
    rootfs_prepare();
    syslog_configure();
    read_some_data();

    unmap_sections();

    const char* init_path = param_value_or_get(KERNEL_PARAM("init"), "/bin/init");

    const char* const argv[] = {init_path, ptr(cmdline), NULL, };
    const char* const envp[] = {"PHOENIX=TRUE", NULL};

    // 1
    timeval_t t = {.tv_sec = 5, .tv_usec = 0};
    ktimer_create_and_start(KTIMER_ONESHOT, t, &timer_callback, NULL);

    // 2
    t.tv_sec = 4;
    ktimer_create_and_start(KTIMER_ONESHOT, t, &timer_callback, NULL);

    // 3
    t.tv_sec = 7;
    ktimer_create_and_start(KTIMER_ONESHOT, t, &timer_callback, NULL);

    // 4
    t.tv_sec = 8;
    ktimer_create_and_start(KTIMER_ONESHOT, t, &timer_callback, NULL);

    // 5
    t.tv_sec = 2;
    ktimer_create_and_start(KTIMER_ONESHOT, t, &timer_callback, NULL);

    // 6
    t.tv_sec = 20;
    ktimer_create_and_start(KTIMER_REPEATING, t, &timer_callback, NULL);

    if (unlikely(do_exec(init_path, argv, envp)))
    {
        panic("cannot run %s", init_path);
    }

    ASSERT_NOT_REACHED();
}
