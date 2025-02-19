#include <kernel/vm.h>
#include <kernel/path.h>
#include <kernel/dentry.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/vm_print.h>
#include <kernel/backtrace.h>

struct user_address
{
    uint32_t vaddr;
    uint32_t file_offset;
    char     path[PATH_MAX];
};

typedef struct user_address user_address_t;

struct bt_data
{
    uintptr_t stack_start, stack_end;
    uintptr_t sp;
    uintptr_t ip;
    stack_frame_t* frame;
    vm_area_t* vm_areas;
};

typedef struct bt_data bt_data_t;

#define is_within(a, start, end) \
    ({ addr(a) >= start && addr(a) < end; })

static void* backtrace_user_start(process_t* p, uintptr_t ip, uintptr_t sp, uintptr_t bp)
{
    bt_data_t* data = fmalloc(sizeof(struct bt_data));

    if (unlikely(!data))
    {
        return NULL;
    }

    data->stack_start = p->mm->stack_start;
    data->stack_end = p->mm->stack_end;
    data->vm_areas = p->mm->vm_areas;
    data->sp = sp;
    data->ip = ip;

    if (likely(is_within(bp, data->stack_start, data->stack_end)))
    {
        data->frame = ptr(bp);
    }
    else
    {
        data->frame = NULL;
    }

    return data;
}

static int address_fill(uintptr_t ip, vm_area_t* vm_areas, user_address_t* addr)
{
    vm_area_t* vma = vm_find(ip, vm_areas);

    if (unlikely(!vma))
    {
        return 1;
    }

    addr->vaddr = ip;
    addr->file_offset = vma->dentry
        ? ip - vma->start + vma->offset
        : ip;

    vm_file_path_read(vma, addr->path, PATH_MAX);

    return 0;
}

static void backtrace_user_format(user_address_t* addr, char* buffer, size_t size)
{
    snprintf(buffer, size, "USER:%#010x %#010x %s:", addr->vaddr, addr->file_offset, addr->path);
}

static void* backtrace_user_next(void** data_ptr, user_address_t* addr)
{
    void* ret;
    bt_data_t* data = *data_ptr;

    if (data->ip)
    {
        ret = ptr(data->ip);
        data->ip = 0;

        if (address_fill(addr(ret), data->vm_areas, addr))
        {
            return NULL;
        }

        return ret;
    }

    if (!is_within(data->frame, data->stack_start, data->stack_end) ||
        address_fill(addr(ret = data->frame->ret - sizeof(uintptr_t)), data->vm_areas, addr))
    {
        return NULL;
    }

    data->frame = data->frame->next;

    return ret;
}

static void backtrace_user_end(void* data)
{
    ffree(data, sizeof(struct bt_data));
}

void backtrace_user(loglevel_t severity, const pt_regs_t* regs, const char* prefix)
{
    char buffer[BACKTRACE_SYMNAME_LEN];
    log(severity, "%sbacktrace: ", prefix);
    user_address_t addr;
    void* data = backtrace_user_start(process_current, PT_REGS_IP(regs), PT_REGS_SP(regs), PT_REGS_BP(regs));

    if (likely(data))
    {
        unsigned depth = 0;
        while ((backtrace_user_next(&data, &addr)) && depth < BACKTRACE_MAX_RECURSION)
        {
            backtrace_user_format(&addr, buffer, sizeof(buffer));
            log(severity, "%s%s", prefix, buffer);
            ++depth;
        }
        backtrace_user_end(data);
    }
}
