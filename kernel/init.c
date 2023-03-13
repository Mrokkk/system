#include <kernel/cpu.h>
#include <kernel/init.h>
#include <kernel/time.h>
#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/module.h>
#include <kernel/unistd.h>
#include <kernel/process.h>
#include <kernel/sections.h>

#include <arch/multiboot.h>

static int init(const char* cmdline);

unsigned init_in_progress = INIT_IN_PROGRESS;

typedef struct options
{
    char syslog[32];
    char init[64];
} options_t;

__noreturn static void idle()
{
    process_stop(process_current);
    for (;; halt());
}

__noreturn void kmain(struct multiboot_info* bootloader_data, uint32_t bootloader_magic)
{
    memset(_sbss, 0, addr(_ebss) - addr(_sbss));

    const char* cmdline = multiboot_read(
        bootloader_data,
        bootloader_magic);

    log_info("cmdline: %s", cmdline);
    log_info("RAM: %u MiB (%u B)", ram / 1024 / 1024, ram);
    memory_areas_print();

    arch_setup();
    paging_init();
    kmalloc_init();
    fmalloc_init();
    irqs_configure();
    processes_init();
    vfs_init();
    modules_init();

    ASSERT(init_in_progress == INIT_IN_PROGRESS);

    sti();
    delay_calibrate();

    init_in_progress = 0;

    // Create another process called 'init' and change itself into the idle process
    kernel_process(init, ptr(cmdline), 0);
    idle();
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
    if ((errno = mount("none", "/", "ramfs", 0, 0)))
    {
        panic("cannot mount root; errno = %d", errno);
    }

    if ((errno = chdir("/")))
    {
        panic("failed to chdir; errno = %d", errno);
    }

    mkdir("/test", 0);
    mkdir("/test/test1", 0);
    mkdir("/test/test1/test2", 0);

    if (mkdir("/dev", 0) || mkdir("/bin", 0))
    {
        panic("cannot create required directories");
    }

    if ((errno = mount("none", "/dev", "devfs", 0, 0)))
    {
        panic("cannot mount devfs; errno = %d", errno);
    }

    if ((errno = mount("none", "/bin", "mbfs", 0, 0)))
    {
        panic("cannot mount mbfs; errno = %d", errno);
    }
}

static inline const char* parse_key_value(
    char* dest,
    const char* src,
    const char* key)
{
    if (!strncmp(key, src, strlen(key)))
    {
        src = strchr(src, '=') + 1;
        const char* next = strchr(src, ' ');
        size_t len = strlen(src);

        char* string = strncpy(
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

static inline void cmdline_parse(options_t* options, const char* cmdline)
{
    const char* temp = cmdline;

    while (*temp)
    {
        temp = parse_key_value(options->syslog, temp, "syslog=");
        temp = parse_key_value(options->init, temp, "init=");
        ++temp;
    }
}

static inline void syslog_configure(const char* device)
{
    int errno;
    file_t* file;

    log_info("device = %s", device);

    if ((errno = do_open(&file, device, O_RDWR, 0)))
    {
        log_error("failed to open syslog output device: %s", device);
    }

    printk_register(file);
}

__noreturn static int init(const char* cmdline)
{
    options_t options = {
        .syslog = "/dev/tty0",
        .init = "/bin/init",
    };

    cmdline_parse(&options, cmdline);
    rootfs_prepare();
    syslog_configure(options.syslog);

#if 1
    extern int debug_monitor();
    kernel_process(&debug_monitor, 0, 0);
#endif

    const char* const argv[] = {options.init, ptr(cmdline), NULL, };
    if (unlikely(do_exec(options.init, argv)))
    {
        panic("cannot run %s", options.init);
    }

    ASSERT_NOT_REACHED();
}
