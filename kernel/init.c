#define log_fmt(fmt) "init: " fmt
#include <stdarg.h>
#include <kernel/cpu.h>
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

#include <arch/pci.h>
#include <arch/vbe.h>
#include <arch/earlycon.h>
#include <arch/multiboot.h>

static int init(const char* cmdline);

unsigned init_in_progress = INIT_IN_PROGRESS;
char cmdline[128];
static param_t* params;

NOINLINE static void NORETURN(run_init_and_go_idle(void))
{
    sti();
    kernel_process_spawn(&init, cmdline, NULL, SPAWN_KERNEL);
    process_stop(process_current);
    scheduler();
    for (;; halt());
}

UNMAP_AFTER_INIT static void params_read(char* tmp_cmdline, param_t* output)
{
    strcpy(tmp_cmdline, cmdline);

    char* save_ptr;
    char* tmp = tmp_cmdline;
    char* previous_token = NULL;
    char* new_token = NULL;
    size_t count = 0;

    do
    {
        new_token = strtok_r(tmp, "= ", &save_ptr);
        if (previous_token)
        {
            bool value_param = cmdline[new_token - tmp_cmdline - 1] == '=';
            output[count].name      = previous_token;
            output[count++].value   = value_param ? new_token : NULL;
            previous_token          = value_param ? NULL : new_token;
        }
        else
        {
            previous_token = new_token;
        }
        tmp = NULL;
    }
    while (new_token);
}

param_t* param_get(const char* name)
{
    for (param_t* p = params; p->name; ++p)
    {
        if (!strcmp(p->name, name))
        {
            return p;
        }
    }
    return NULL;
}

const char* param_value_get(const char* name)
{
    param_t* param = param_get(name);
    return param ? param->value : NULL;
}

const char* param_value_or_get(const char* name, const char* def)
{
    param_t* param = param_get(name);
    return param ? param->value : def;
}

int param_bool_get(const char* name)
{
    return !!param_get(name);
}

void param_call_if_set(const char* name, int (*action)())
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

UNMAP_AFTER_INIT void boot_params_print(void)
{
    log_notice("bootloader: %s", bootloader_name);
    log_notice("cmdline: %s", cmdline);
}

UNMAP_AFTER_INIT void ram_print(void)
{
    uint32_t ram_hi = addr(full_ram >> 32);
    uint32_t ram_low = addr(full_ram & ~0UL);
    uint32_t mib = 4096 * ram_hi + ram_low / MiB;
    log_notice("RAM: %u MiB", mib);
    log_notice("RAM end: %u MiB (%u B)", ram / MiB, ram);
}

UNMAP_AFTER_INIT void NORETURN(kmain(void* data, ...))
{
    bss_zero();

    char tmp_cmdline[128];
    param_t parameters[32];
    params = parameters;

    {
        va_list args;
        va_start(args, data);
        const char* temp_cmdline = multiboot_read(args);
        va_end(args);
        strcpy(cmdline, temp_cmdline);
    }

    params_read(tmp_cmdline, parameters);

    boot_params_print();

    arch_setup();

    ram_print();
    memory_areas_print();
    sections_print();

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

    timeval_t ts;
    timestamp_get(&ts);

    if (ts.tv_sec || ts.tv_usec)
    {
        log_notice("boot finished in %u.%06u s", ts.tv_sec, ts.tv_usec);
    }

    run_init_and_go_idle();

    ASSERT_NOT_REACHED();
}

// I left this comment even though this issue no longer exists (as init is doing exec("/bin/init")).
// FIXME: idea to fork in kernel with kernel stacks mapped to different vaddr is broken.
// In such situation any register might reference a previous stack. In this case argv for exec was passed from ebx, which
// had value referencing parent's stack
//
// │    0xc0107f2d <init+605>   push   %eax
// │    0xc0107f2e <init+606>   xor    %ecx,%ecx        ---> setting ecx as 0
// │    0xc0107f30 <init+608>   push   %eax
// │    0xc0107f31 <init+609>   push   %ebx             ---> second param for exec, which is address from previous stack
// │    0xc0107f32 <init+610>   push   $0xc0113aec      ---> first param for exec
// │    0xc0107f37 <init+615>   mov    %ecx,-0x18(%ebp) ---> zeroing first element in array which is supposed to be argv
// │    0xc0107f3a <init+618>   call   0xc010ef60 <exec>
//
// When this is non-inline, compiler will generate new frame, thus argv will be properly filled

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
            if ((errno = mount(sources[i], "/", fs_types[j], 0, 0)))
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

UNMAP_AFTER_INIT static void rootfs_prepare(void)
{
    int errno;

    if (unlikely((errno = root_mount())))
    {
        panic("cannot mount root: %d", errno);
    }

    if ((errno = chroot("/")))
    {
        panic("cannot set root; errno = %d", errno);
    }

    if ((errno = chdir("/")))
    {
        panic("failed to chdir; errno = %d", errno);
    }

    if ((errno = mount("none", "/dev", "devfs", 0, 0)))
    {
        panic("cannot mount devfs; errno = %d", errno);
    }

    if ((errno = mount("none", "/proc", "proc", 0, 0)))
    {
        panic("cannot mount proc; errno = %d", errno);
    }

    if ((errno = mount("none", "/tmp", "ramfs", 0, 0)))
    {
        log_warning("cannot mount ramfs on /tmp");
    }
}

UNMAP_AFTER_INIT static void syslog_configure(void)
{
    int errno;
    file_t* file;
    const char* device = param_value_get(KERNEL_PARAM("syslog"));

    log_notice("syslog output: %s", device ? device : "none");

    if (!device || !strcmp(device, "none"))
    {
        return;
    }

    if ((errno = do_open(&file, device, O_RDWR, 0)))
    {
        log_error("failed to open syslog output device: %s", device);
    }

    printk_register(file);
}

UNMAP_AFTER_INIT static void read_some_data(void)
{
    int errno;
    scoped_file_t* file = NULL;
    char* buf = single_page();
    const char* dev = "/dev/sda";

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
        memory_dump(log_info, buf, 5);
        memory_dump(log_info, buf + 508, 1);
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

static int NORETURN(init(const char* cmdline))
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
    ktimer_create(KTIMER_ONESHOT, t, &timer_callback, NULL);

    // 2
    t.tv_sec = 4;
    ktimer_create(KTIMER_ONESHOT, t, &timer_callback, NULL);

    // 3
    t.tv_sec = 7;
    ktimer_create(KTIMER_ONESHOT, t, &timer_callback, NULL);

    // 4
    t.tv_sec = 8;
    ktimer_create(KTIMER_ONESHOT, t, &timer_callback, NULL);

    // 5
    t.tv_sec = 2;
    ktimer_create(KTIMER_ONESHOT, t, &timer_callback, NULL);

    // 6
    t.tv_sec = 20;
    ktimer_create(KTIMER_REPEATING, t, &timer_callback, NULL);

    if (unlikely(do_exec(init_path, argv, envp)))
    {
        panic("cannot run %s", init_path);
    }

    ASSERT_NOT_REACHED();
}
