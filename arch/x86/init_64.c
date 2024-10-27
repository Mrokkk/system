#ifdef __x86_64__
#include <arch/io.h>
#include <arch/asm.h>
#include <arch/nmi.h>
#include <arch/pci.h>
#include <arch/bios.h>
#include <arch/memory.h>
#include <arch/bios32.h>
#include <arch/memory.h>
#include <arch/multiboot.h>
#include <arch/real_mode.h>
#include <arch/descriptor.h>

#include <kernel/cpu.h>
#include <kernel/init.h>
#include <kernel/init.h>
#include <kernel/time.h>
#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/page_alloc.h>
#include <kernel/page_table.h>

// FIXME: This file is temporary until long mode paging is implemented

struct cpu_info cpu_info;
extern uintptr_t __stack_chk_guard;
uintptr_t __stack_chk_guard __attribute__((used)) = 0xf392849583;
uint8_t init_stack[PAGE_SIZE];
unsigned init_in_progress = INIT_IN_PROGRESS;
char cmdline[CMDLINE_SIZE];
char* bootloader_name;
void* process_current;
static param_t* params;
extern uint64_t pgt4[];
page_t* page_map;

struct printk_state
{
    logseq_t   sequence;
    off_t      start;
    off_t      end;
    off_t      gap;
    off_t      cur;
    off_t      prev_cur;
    loglevel_t prev_loglevel;
    int        initialized;
};

static struct printk_state CACHELINE_ALIGN state;

void NORETURN(__stack_chk_fail(void))
{
    __builtin_trap();
}

#define PRINTK_LINE_LEN 1024

static void write_log(loglevel_t, char* buffer, size_t len, int)
{
    while (len--)
    {
        outb(*(buffer++), 0xe9);
    }
}

void __pages_free(page_t*)
{
}

logseq_t printk(const printk_entry_t* entry, const char* fmt, ...)
{
    timeval_t ts = {};
    va_list args;
    int len, content_start;
    char buffer[PRINTK_LINE_LEN];

    scoped_irq_lock();

    logseq_t current_seq = state.sequence;

    switch (entry->log_level)
    {
        case KERN_DEBUG:
            len = snprintf(buffer, PRINTK_LINE_LEN, "%u,%lu,%u.%06u;%s:%u:%s: ",
                KERN_DEBUG,
                state.sequence++,
                ts.tv_sec,
                ts.tv_usec,
                entry->file,
                entry->line,
                entry->function);
            break;
        case KERN_CONT:
            len = snprintf(buffer, PRINTK_LINE_LEN, " ");
            break;
        default:
            len = snprintf(buffer, PRINTK_LINE_LEN, "%u,%lu,%u.%06u;",
                entry->log_level,
                state.sequence++,
                ts.tv_sec,
                ts.tv_usec);
            break;
    }

    content_start = len;

    va_start(args, fmt);
    len += vsnprintf(buffer + len, PRINTK_LINE_LEN - len, fmt, args);
    va_end(args);

    len += snprintf(buffer + len, PRINTK_LINE_LEN - len, "\n");

    if (unlikely(len >= PRINTK_LINE_LEN))
    {
        strcpy(buffer + PRINTK_LINE_LEN - 5, "...\n");
    }

    state.cur += len;

    write_log(entry->log_level, buffer, len, content_start);

    if (entry->log_level != KERN_CONT)
    {
        state.prev_loglevel = entry->log_level;
    }

    return current_seq;
}

void do_exception(uintptr_t nr, const pt_regs_t* regs)
{
    log_critical("Exception %u error code %x cs: %x!", nr, regs->error_code, regs->cs);
    if (nr == 14)
    {
        uintptr_t cr2 = cr2_get();
        log_critical("cr2: %p", cr2);
    }
    regs_print(KERN_CRIT, regs, "kernel");
    halt();
}

void timestamp_update()
{
}

void do_irq()
{
}

void syscall()
{
    log_info(__func__);
}

UNMAP_AFTER_INIT static void memory_print(void)
{
    log_notice("RAM: %u MiB", full_ram >> 20);

    if (full_ram != (uint64_t)usable_ram)
    {
        log_continue("; usable %lu MiB (%lu B)", usable_ram / MiB, usable_ram);
    }

    memory_areas_print();
    sections_print();
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
            bool value_param      = new_token ? cmdline[new_token - buffer - 1] == '=' : false;
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

void* slab_alloc(size_t size)
{
    static void* end;
    void* prev;

    if (!end)
    {
        prev = end = ptr(page_align(addr(sections[6].end)));
    }
    else
    {
        prev = end;
        end += align(size, 16);
    }

    return prev;
}

void backtrace_dump(loglevel_t)
{
}

void kmain(void* data, ...)
{
    va_list args;
    const char* temp_cmdline;
    char buffer[CMDLINE_SIZE];
    param_t parameters[CMDLINE_PARAMS_COUNT] = {0};

    va_start(args, data);
    temp_cmdline = multiboot_read(args);
    va_end(args);

    strlcpy(cmdline, temp_cmdline, sizeof(cmdline));
    strlcpy(buffer, temp_cmdline, sizeof(buffer));

    boot_params_print();

    real_mode_call_init();

    idt_init();
    tss_init();
    nmi_enable();
    cpu_detect();
    memory_detect();
    bios32_init();

    memory_print();

    params = params_read(buffer, parameters);

    extern void protected_mode_enter();

    uint64_t* long_pgt4 = virt_ptr(pgt4);
    long_pgt4[0] = 0;

    tlb_flush();

    asm volatile("int $0x80");

    pci_scan();

    halt();
}
#endif
