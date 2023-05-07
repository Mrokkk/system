#define log_fmt(fmt) "init: " fmt
#include <stdarg.h>
#include <kernel/cpu.h>
#include <kernel/init.h>
#include <kernel/time.h>
#include <kernel/devfs.h>
#include <kernel/ksyms.h>
#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/module.h>
#include <kernel/procfs.h>
#include <kernel/unistd.h>
#include <kernel/process.h>
#include <kernel/sections.h>
#include <kernel/backtrace.h>

#include <arch/pci.h>
#include <arch/vbe.h>
#include <arch/earlycon.h>
#include <arch/multiboot.h>

void ide_dma(void);
static int init(const char* cmdline);

unsigned init_in_progress = INIT_IN_PROGRESS;
char cmdline[128];

typedef struct options
{
    char syslog[32];
    char init[64];
} options_t;

static options_t options = {
    .syslog = "/dev/tty0",
    .init = "/bin/init",
};

NOINLINE static void NORETURN(run_init_and_go_idle())
{
    sti();
    kernel_process_spawn(&init, cmdline, NULL, SPAWN_KERNEL);
    process_stop(process_current);
    scheduler();
    for (;; halt());
}

UNMAP_AFTER_INIT void NORETURN(kmain(void* data, ...))
{
    bss_zero();

    va_list args;
    va_start(args, data);
    const char* temp_cmdline = multiboot_read(args);
    va_end(args);

    strcpy(cmdline, temp_cmdline);

    log_notice("bootloader: %s", bootloader_name);
    log_notice("cmdline: %s", cmdline);

    if (strstr(cmdline, "earlycon"))
    {
        earlycon_init();
    }

    arch_setup();

    uint32_t ram_hi = addr(full_ram >> 32);
    uint32_t ram_low = addr(full_ram & ~0UL);
    uint32_t mib = 4096 * ram_hi + ram_low / MiB;
    log_notice("RAM: %u MiB", mib);
    log_notice("RAM end: %u MiB (%u B)", ram / MiB, ram);
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

    ts_t ts;
    timestamp_get(&ts);

    if (ts.seconds || ts.useconds)
    {
        log_notice("boot finished in %u.%06u s", ts.seconds, ts.useconds);
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

static inline void rootfs_prepare()
{
    int errno;
    char* sources[] = {"/dev/hda0", "/dev/img0"};

    for (size_t i = 0; i < array_size(sources); ++i)
    {
        log_info("mounting root on %s", sources[i]);
        if ((errno = mount(sources[i], "/", "ext2", 0, 0)))
        {
            log_info("mounting ext2 on %s failed with %d", sources[i], errno);
        }
        else
        {
            break;
        }
    }

    if (unlikely(errno))
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

static const char* parse_key_value(char* dest, const char* src, const char* key)
{
    size_t len;
    char* string;
    const char* next;

    if (!strncmp(key, src, strlen(key)))
    {
        src = strchr(src, '=') + 1;
        next = strchr(src, ' ');
        len = strlen(src);

        string = strncpy(
            dest,
            src,
            next ? (size_t)(next - src) : len);

        if (next)
        {
            src = next + 1;
        }
        else
        {
            src += len;
        }
        *string = 0;
    }

    return src;
}

static inline void cmdline_parse(const char* cmdline)
{
    const char* temp = cmdline;

    while (*temp)
    {
        temp = parse_key_value(options.syslog, temp, "syslog=");
        temp = parse_key_value(options.init, temp, "init=");
        ++temp;
    }
}

static inline void syslog_configure()
{
    int errno;
    file_t* file;
    const char* device = options.syslog;

    log_notice("syslog output: %s", device);

    if (!strcmp(device, "none"))
    {
        return;
    }

    if ((errno = do_open(&file, device, O_RDWR, 0)))
    {
        log_error("failed to open syslog output device: %s", device);
    }

    printk_register(file);
}

static inline void unmap_sections()
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

static int NORETURN(init(const char* cmdline))
{
    cmdline_parse(cmdline);
    rootfs_prepare();
    syslog_configure();

    {
        int errno;
        scoped_file_t* file = NULL;
        char* buf = single_page();

        memset(buf, 0, PAGE_SIZE);

        if ((errno = do_open(&file, "/dev/hda", O_RDONLY, 0)))
        {
            log_info("cannot open /dev/hda");
        }
        else
        {
            log_info("reading data");
            do_read(file, 0, buf, PAGE_SIZE);
            memory_dump(log_info, buf + 508, 1);
        }

        page_free(buf);
    }

    unmap_sections();

#if 0
    pci_devices_list();
#endif

#if 0
    video_modes_print();
#endif
#if 1
    extern int debug_monitor();
    kernel_process_spawn(&debug_monitor, NULL, NULL, SPAWN_KERNEL);
#endif

    const char* const argv[] = {options.init, ptr(cmdline), NULL, };
    if (unlikely(do_exec(options.init, argv)))
    {
        panic("cannot run %s", options.init);
    }

    ASSERT_NOT_REACHED();
}
