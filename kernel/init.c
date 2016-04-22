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

void kmain();
static void idle();
static void welcome();
int init();

struct cpu_info cpu_info;

volatile unsigned int jiffies;

char kernel_status = 0;
struct mmap mmap[10];
unsigned int ram;
struct kernel_symbol *kernel_symbols[370];
int kernel_symbols_size;

/* FIXME: Specify interface for reading RAM */
/* FIXME: Add better support for boot modules */

/*===========================================================================*
 *                                   kmain                                   *
 *===========================================================================*/
__noreturn void kmain() {

    ASSERT_EQ(Check_Status, kernel_status, 0);

    printk("RAM: %u MiB (%u B)\n", ram/1024/1024, ram);

    processes_init();

    kernel_process(&init, "init");

    kernel_status = 1;

    idle();

    while (1);

}

/*===========================================================================*
 *                                   idle                                    *
 *===========================================================================*/
__noreturn static void idle() {

    process_stop(process_current);
    while (1);

}

/*===========================================================================*
 *                                  welcome                                  *
 *===========================================================================*/
static void welcome() {

    printf("myOS v%d.%d for %s\n",
    VERSION_MAJ, VERSION_MIN, stringify(ARCH));
#if DEBUG
    printf("Debug=%d\n", DEBUG);
#endif
    printf("Builded on %s %s\n",
    COMPILE_SYSTEM, COMPILE_ARCH);
    printf("Compiled on %sby %s@%s\n", COMPILER, COMPILE_BY, COMPILE_HOST);
    printf("Copyright (C) 2016, MP\n\n");
    printf("Welcome!\n\n");

}

int temp_shell();

/*===========================================================================*
 *                                   init                                    *
 *===========================================================================*/
int init() {

    /*
     * Whole is temporary while there's no real fs and binary loading...
     */

    int child_pid, status;
    char arch_info[128], modules_list[128], irqs_list[128],
            char_devices_list[128], block_devices_list[128];

    (void)arch_info; (void)modules_list; (void)irqs_list;
    (void)char_devices_list; (void)block_devices_list;

    modules_init();

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
    block_devices_list_get(block_devices_list);
    printk("%s%s", char_devices_list, block_devices_list);
#endif

    printf("This shouldn't be seen\n");

    /* Open standard streams */
    if (open("/dev/tty0", 0) || dup(0) || dup(0)) exit(-1);

    /* Say hello */
    welcome();

    /* Just playing with serial */
    if (open("/dev/ttyS0", 0)) printf("Cannot open serial\n");
    fprintf(3, "Hello World!\n");
    if (close(3)) printf("Can't close ttyS0\n");
    fprintf(3, "Hello World!\n");

    /* Do fork */
    if ((child_pid = fork()) < 0) {
        printf("Fork error in init!\n");
        exit(-1);
    } else if (!child_pid) {
        exit(temp_shell());
    }

    waitpid(child_pid, &status, 0);

    exit(0);

    return 0;

}

/*===========================================================================*
 *                                 read_line                                 *
 *===========================================================================*/
static inline void read_line(char *line) {

    int size = read(0, line, 32);
    line[size-1] = 0;

}

/*===========================================================================*
 *                                  c_zombie                                 *
 *===========================================================================*/
static int c_zombie() {

    int pid = fork();

    return pid;

}

/*===========================================================================*
 *                                   c_bug                                   *
 *===========================================================================*/
static int c_bug() {

    exec();
    printf("Bug!!!\n");
    printk("It's a bug!");

    return 0;

}

#define COMMAND(name) {#name, c_##name}

static struct command {
    char *name;
    int (*function)();
} commands[] = {
        COMMAND(zombie),
        COMMAND(bug),
        {0, 0}
};

/*===========================================================================*
 *                                temp_shell                                 *
 *===========================================================================*/
int temp_shell() {

    char line[32];
    struct command *com = commands;
    int i, pid, status = 0;

    while (1) {
        status ? printf("* ") : printf("# ");
        read_line(line);
        if (line[0] == 0) continue;
        if ((pid=fork()) == 0) {
            for (i=0; com[i].name; i++) {
                if (!strcmp(com[i].name, line))
                    return com[i].function();
            }
            if (!com[i].name) printf("No such command: %s\n", line);
            return -EBADC;
        } else if (pid < 0) printf("%s: fork error!\n", line);
        waitpid(pid, &status, 0);
    }

    return 0;

}

char *word_read(char *string, char *output) {

    while(*string != '\n' && *string != '\0'
                && *string != ' ') {
        *output++ = *string++;
    }

    string++;
    *output = 0;

    return string;

}

unsigned int strhex2uint(char *s) {

    unsigned int result = 0;
    int c ;

    if ('0' == *s && 'x' == *(s+1)) {
        s+=2;
        while (*s) {
            result = result << 4;
            if (c = (*s - '0'), (c>=0 && c <=9)) result|=c;
            else if (c = (*s-'A'), (c>=0 && c <=5)) result|=(c+10);
            else if (c = (*s-'a'), (c>=0 && c <=5)) result|=(c+10);
            else break;
            ++s;
        }
    }
    return result;
}

char *symbol_read(char *temp, struct kernel_symbol *symbol) {

    char str_address[16], str_size[16],
         str_type[4], str_hex[16];
    char *temp2;

    temp = word_read(temp, symbol->name);
    temp = word_read(temp, str_address);
    temp = word_read(temp, str_size);
    temp = word_read(temp, str_type);

    sprintf(str_hex, "0x%s", str_address);
    symbol->address = strhex2uint(str_hex);
    symbol->type = *str_type;
    symbol->size = strtol(str_size, &temp2, 10);

    return temp;

}

struct kernel_symbol *symbol_find(const char *name) {

    int i;

    for (i = 0; i < kernel_symbols_size; i++) {
        if (!strncmp(kernel_symbols[i]->name, name, 32))
            return kernel_symbols[i];
    }

    return 0;

}

struct kernel_symbol *symbol_find_address(unsigned int address) {

    int i;

    for (i = 0; i < kernel_symbols_size; i++) {
        if ((address < kernel_symbols[i]->address + kernel_symbols[i]->size)
                && (address > kernel_symbols[i]->address))
            return kernel_symbols[i];
    }

    return 0;

}

int symbols_read(char *symbols, unsigned int size) {

    char *temp = symbols;
    int nr = 0;

    while ((unsigned int)temp < (unsigned int)symbols + size) {
        kernel_symbols[nr] = kmalloc(sizeof(struct kernel_symbol));
        if (!kernel_symbols[nr]) {
            printk("Out of memory!\n");
            break;
        }
        temp = symbol_read(temp, kernel_symbols[nr]);
        nr++;
    }

    printk("Read %d symbols\n", nr);
    kernel_symbols_size = nr;

    return 0;

}


