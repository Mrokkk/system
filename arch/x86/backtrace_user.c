#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/backtrace.h>

typedef struct bt_data
{
    uint32_t code_start, code_end;
    uint32_t stack_start, stack_end;
    uint32_t esp;
    uint32_t eip;
    stack_frame_t* frame;
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

void* backtrace_user_next(void** data_ptr)
{
    void* ret;
    bt_data_t* data = *data_ptr;

    if (data->eip)
    {
        ret = ptr(data->eip);
        data->eip = 0;
        return ret;
    }

    if (!is_within(data->frame, data->stack_start, data->stack_end))
    {
        ffree(data, sizeof(struct bt_data));
        return NULL;
    }

    ret = data->frame->ret;

    if (!is_within(addr(ret), data->code_start, data->code_end))
    {
        ffree(data, sizeof(struct bt_data));
        return NULL;
    }

    data->frame = data->frame->next;

    return ret;
}
