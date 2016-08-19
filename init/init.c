#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/device.h>
#include <kernel/irq.h>
#include <kernel/mm.h>
#include <kernel/module.h>
#include <kernel/unistd.h>
#include <kernel/fs.h>
#include <kernel/time.h>
#include <kernel/test.h>

#include <arch/register.h>
#include <arch/segment.h>

static int init(void);
void irqs_configure(void);
int paging_init(void);
int temp_shell(void);

struct cpu_info cpu_info;

volatile unsigned int jiffies;

struct mmap mmap[10];
unsigned int ram;

/* FIXME: Specify interface for reading RAM */
/* FIXME: Add better support for boot modules */

unsigned long loops_per_sec = (1<<12);

/* This is the number of bits of precision for the loops_per_second.  Each
   bit takes on average 1.5/HZ seconds.  This (like the original) is a little
   better than 1% */
#define LPS_PREC 8

/*===========================================================================*
 *                              delay_calibrate                              *
 *===========================================================================*/
static void delay_calibrate(void) {

    unsigned int ticks;
    int loopbit;
    int lps_precision = LPS_PREC;
    int flags;

    loops_per_sec = (1<<12);

    irq_save(flags);
    sti();

    printk("Calibrating delay loop.. ");
    while (loops_per_sec <<= 1) {
        /* wait for "start of" clock tick */
        ticks = jiffies;
        while (ticks == jiffies)
            /* nothing */;
        /* Go .. */
        ticks = jiffies;
        do_delay(loops_per_sec);
        ticks = jiffies - ticks;
        if (ticks)
            break;
    }

    /* Do a binary approximation to get loops_per_second set to equal one clock
     (up to lps_precision bits) */
    loops_per_sec >>= 1;
    loopbit = loops_per_sec;
    while (lps_precision-- && (loopbit >>= 1) ) {
        loops_per_sec |= loopbit;
        ticks = jiffies;
        while (ticks == jiffies);
        ticks = jiffies;
        do_delay(loops_per_sec);
        if (jiffies != ticks)    /* longer than 1 tick */
            loops_per_sec &= ~loopbit;
    }

/* finally, adjust loops per second in terms of seconds instead of clocks */
    loops_per_sec *= HZ;
/* Round the value and print it */
    printk("ok - %lu.%02lu BogoMIPS\n",
        (loops_per_sec+2500)/500000,
        ((loops_per_sec+2500)/5000) % 100);

    irq_restore(flags);

}

/*===========================================================================*
 *                                   idle                                    *
 *===========================================================================*/
__noreturn static void idle() {

    /*
     * Remove itself from the running queue
     */

    process_stop(process_current);
    while (1);

}

/*===========================================================================*
 *                                   kmain                                   *
 *===========================================================================*/
__noreturn void kmain(char *boot_params) {

    printk("Boot params: %s\n", boot_params);

    arch_setup();
    paging_init();
    irqs_configure();
    delay_calibrate();
    processes_init();
    vfs_init();

    sti();
    /* Now we're officially in the first process with
     * PID 0 (init_process).
     */

    /* Create another process called 'init' (ugh...)
     * and change itself into the idle process.
     */
    kernel_process(init, 0, 0);
    idle();

}

/*===========================================================================*
 *                                  welcome                                  *
 *===========================================================================*/
static void welcome() {

    printk("myOS v%d.%d for %s\n",
    VERSION_MAJ, VERSION_MIN, stringify(ARCH));
#if DEBUG
    printk("Debug=%d\n", DEBUG);
#endif
    printk("Builded on %s %s by %s@%s\n",
    COMPILE_SYSTEM, COMPILE_ARCH, COMPILE_BY, COMPILE_HOST);
    printk("Compiled on %s\n", COMPILER);
    printk("Copyright (C) 2016, MP\n\n");
    printk("Welcome!\n\n");

}

/*===========================================================================*
 *                              hardware_print                               *
 *===========================================================================*/
static inline void hardware_print() {

    char arch_info[128], modules_list[128], irqs_list[128],
            char_devices_list[128];

    (void)arch_info; (void)modules_list; (void)irqs_list;
    (void)char_devices_list;

#ifdef CONFIG_PRINT_ARCH
    arch_info_get(arch_info);
    printk("%s", arch_info);
#endif
#ifdef CONFIG_PRINT_IRQS
    irqs_list_get(irqs_list);
    printk("%s", irqs_list);
#endif
#ifdef CONFIG_PRINT_MODULES
    modules_list_get(modules_list);
    printk("%s", modules_list);
#endif
#ifdef CONFIG_PRINT_DEVICES
    char_devices_list_get(char_devices_list);
    printk("%s", char_devices_list);
#endif

    printk("RAM: %u MiB (%u B)\n", ram/1024/1024, ram);

}

/*===========================================================================*
 *                                 root_mount                                *
 *===========================================================================*/
static inline int root_mount() {
    return mount("none", "/", "rootfs", 0, 0);
}

/*===========================================================================*
 *                                console_open                               *
 *===========================================================================*/
static inline int console_open() {

#ifdef CONFIG_SERIAL_PRIMARY
    char *console = "/dev/ttyS0";
#else
    char *console = "/dev/tty0";
#endif
    if (open(console, 0) || dup(0) || dup(0))
        return 1;
    return 0;

}

/*===========================================================================*
 *                                 shell_run                                 *
 *===========================================================================*/
static inline int shell_run() {

    int child_pid, status;

    /* Do fork */
    if ((child_pid = fork()) < 0) {
        printk("Fork error in init!\n");
        return -1;
    } else if (!child_pid) {
        strcpy(process_current->name, "shell");
        exec(&temp_shell);
    }

    waitpid(child_pid, &status, 0);
    while (1);

}

/*===========================================================================*
 *                                   init                                    *
 *===========================================================================*/
__noreturn static int init() {

    /*
     * Whole is temporary while there's no real fs and binary loading...
     */

#ifdef CONFIG_X86
    ASSERT(cs_get() == KERNEL_CS);
    ASSERT(ds_get() == KERNEL_DS);
    ASSERT(gs_get() == KERNEL_DS);
    ASSERT(ss_get() == KERNEL_DS);
#endif

    strcpy(process_current->name, "init");

    modules_init();

    if (root_mount()) panic("Cannot mount root");
    if (console_open()) panic("Cannot open console");

    /* Say something about hardware */
    hardware_print();

    /* Say hello */
    welcome();

#ifdef CI
    reboot();
#endif

    if (shell_run()) panic("Cannot run shell");

    while (1);

}

