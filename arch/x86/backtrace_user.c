#include <kernel/vm.h>
#include <kernel/path.h>
#include <kernel/dentry.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/vm_print.h>
#include <kernel/backtrace.h>

typedef struct bt_data
{
    uint32_t code_start, code_end;
    uint32_t stack_start, stack_end;
    uint32_t esp;
    uint32_t eip;
    stack_frame_t* frame;
    vm_area_t* vm_areas;
} bt_data_t;

#define is_within(a, start, end) \
    ({ addr(a) >= start && addr(a) < end; })

void* backtrace_user_start(struct process* p, uint32_t eip, uint32_t esp, uint32_t ebp)
{
    bt_data_t* data;

    data = fmalloc(sizeof(struct bt_data));
    if (!data)
    {
        return NULL;
    }

    data->code_start = p->mm->code_start;
    data->code_end = p->mm->code_end;
    data->stack_start = p->mm->stack_start;
    data->stack_end = p->mm->stack_end;
    data->vm_areas = p->mm->vm_areas;
    data->esp = esp;
    data->eip = eip;

    if (is_within(ebp, data->stack_start, data->stack_end))
    {
        data->frame = ptr(ebp);
    }
    else
    {
        data->frame = NULL;
    }

    return data;
}

static int address_fill(uint32_t eip, vm_area_t* vm_areas, user_address_t* addr)
{
    vm_area_t* vma = vm_find(addr(eip), vm_areas);

    if (!vma)
    {
        return 1;
    }

    addr->vaddr = eip;
    addr->file_offset = eip - vma->start + vma->offset;

    vm_file_path_read(vma, addr->path, PATH_MAX);

    return 0;
}

void backtrace_user_format(user_address_t* addr, char* buffer)
{
    sprintf(buffer, "USER:%08x %08x %s:", addr->vaddr, addr->file_offset, addr->path);
}

void* backtrace_user_next(void** data_ptr, user_address_t* addr)
{
    void* ret;
    bt_data_t* data = *data_ptr;

    if (data->eip)
    {
        ret = ptr(data->eip);
        data->eip = 0;

        if (address_fill(addr(ret), data->vm_areas, addr))
        {
            ffree(data, sizeof(struct bt_data));
            return NULL;
        }

        return ret;
    }

    if (!is_within(data->frame, data->stack_start, data->stack_end) ||
        address_fill(addr(ret = data->frame->ret - 4), data->vm_areas, addr))
    {
        ffree(data, sizeof(struct bt_data));
        return NULL;
    }

    stack_frame_t* prev = data->frame;
    data->frame = data->frame->next;

    if (data->frame == prev)
    {
        ffree(data, sizeof(struct bt_data));
        return NULL;
    }

    return ret;
}
