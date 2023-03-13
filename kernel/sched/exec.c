#include <kernel/elf.h>
#include <kernel/module.h>
#include <kernel/process.h>

static inline int binary_image_get(const char* pathname, void** data)
{
    int errno;
    file_t* file;

    if ((errno = do_open(&file, pathname, 0, 0))) // FIXME: add flags
    {
        log_debug("failed open");
        return errno;
    }

    if (!file)
    {
        panic("file is %O", file);
    }

    if (!file->ops || !file->ops->mmap)
    {
        log_debug("no ops");
        errno = -ENOOPS;
        goto finish;
    }

    if ((errno = file->ops->mmap(file, data)))
    {
        log_debug("mmap failed");
        goto finish;
    }

    if (unlikely(!*data))
    {
        log_error("bug in mmap; returned data = null");
        errno = -EERR;
        goto finish;
    }

    errno = 0;

finish:
    delete(file, list_del(&file->files));
    return errno;
}

// Memory layout in argv area (0 is also the top of the user stack):
// 0      4      8        12   8+argc*4     12+argc*4
// |------|------|---------|-------|------------|
// | argc | argv | argv[0] |  ...  | argv[argc] |
// |------|------|---------|-------|------------|
static inline void argv_copy(uint32_t* dest, int argc, const char* const argv[], uint32_t virt_addr)
{
    char* temp;
    uint32_t orig_dest = addr(dest);

    // Put argc
    *dest++ = argc;

    // Put argv address
    *dest = virt_addr + addr(dest + 1) - orig_dest;
    dest++;

    // argv will always have size == argc + 1; argv[argc] must be 0
    temp = ptr(dest + argc + 1);

    for (int i = 0; i < argc; ++i)
    {
        // Put argv[i] address and copy content of argv[i]
        *dest++ = virt_addr + addr(temp) - orig_dest;
        temp = strcpy(temp, argv[i]);
    }

    *dest++ = 0;
}

int do_exec(const char* pathname, const char* const argv[])
{
    int errno;
    int argc;
    void* data;
    void* entry;
    vm_area_t* vm_areas = NULL;
    vm_area_t* args_vma;
    vm_area_t* stack_vma;
    uint32_t brk;
    uint32_t user_stack;
    uint32_t* kernel_stack;

    for (argc = 0; argv[argc]; ++argc);

    log_debug("%s, argc=%u, argv=%x", pathname, argc, argv);

    if ((errno = binary_image_get(pathname, &data)))
    {
        return errno;
    }

    entry = read_elf(pathname, data, &vm_areas, &brk);

    if (!entry)
    {
        return -EACCES;
    }

    page_t* page = page_alloc1();

    if (unlikely(!page))
    {
        return -ENOMEM;
    }

    memset(page_virt_ptr(page), 0, PAGE_SIZE);
    void* new_argv = page_virt_ptr(page);

    argv_copy(new_argv, argc, argv, USER_ARGV_VIRT_ADDRESS);

    args_vma = vm_create(
        page,
        USER_ARGV_VIRT_ADDRESS,
        PAGE_SIZE,
        VM_READ);

    if (unlikely(!args_vma))
    {
        page_free(new_argv);
        return -ENOMEM;
    }

    vm_add(&vm_areas, args_vma);

    process_current->mm->args_start = args_vma->virt_address;
    process_current->mm->args_end = vm_virt_end(args_vma);

    pgd_t* pgd = ptr(process_current->mm->pgd);
    log_debug("pgd=%x", pgd);

    strncpy(process_current->name, pathname, PROCESS_NAME_LEN);

    kernel_stack = process_current->mm->kernel_stack; // FIXME: maybe I should alloc a new stack?

    if (process_is_kernel(process_current))
    {
        stack_vma = stack_create(USER_STACK_VIRT_ADDRESS, pgd);
        process_current->context.esp0 = addr(kernel_stack);
        process_current->mm->stack_start = stack_vma->virt_address;
        process_current->mm->stack_end = stack_vma->virt_address + stack_vma->size;
    }
    else
    {
        // Remove previous areas except for stack
        stack_vma = process_stack_vm_area(process_current);
        vm_area_t* prev;
        for (vm_area_t* temp = process_current->mm->vm_areas; temp;)
        {
            prev = temp;
            if (temp != stack_vma)
            {
                vm_remove(temp, pgd, 1);
                temp = temp->next;
                delete(prev);
            }
            else
            {
                temp = temp->next;
                prev->next = NULL;
            }
        }
    }

    process_current->mm->brk = brk;

    for (vm_area_t* temp = vm_areas; temp; temp = temp->next)
    {
        vm_apply(temp, pgd, VM_APPLY_REPLACE_PG);
    }

    pgd_reload();

    user_stack = vm_virt_end(stack_vma);

    vm_add(&vm_areas, stack_vma);

    process_current->mm->vm_areas = vm_areas;
    process_current->type = USER_PROCESS;

    log_debug("proc %d areas:", process_current->pid);
    vm_print(process_current->mm->vm_areas);

    arch_exec(entry, kernel_stack, user_stack);

    return 0;
}
