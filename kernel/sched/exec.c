#include <kernel/elf.h>
#include <kernel/path.h>
#include <kernel/module.h>
#include <kernel/process.h>

static inline int binary_image_get(const char* pathname, void** data)
{
    int errno;
    file_t* file;

    if ((errno = do_open(&file, pathname, O_RDONLY, 0)))
    {
        return errno;
    }

    if (unlikely(!file))
    {
        log_warning("null file returned by open");
        errno = -ENOENT;
        goto finish;
    }

    if (!file->ops || !file->ops->mmap)
    {
        errno = -ENOSYS;
        goto finish;
    }

    if ((errno = file->ops->mmap(file, data)))
    {
        goto finish;
    }

    if (unlikely(!*data))
    {
        struct kernel_module* mod = module_find(file->inode->sb->module);
        log_error("bug in %s module; mmap returned data = null", mod ? mod->name : "<unknown>");
        errno = -EFAULT;
        goto finish;
    }

    errno = 0;

finish:
    delete(file, list_del(&file->files));
    return errno;
}

// Memory layout of args:
// 0      4      8        12   8+argc*4     12+argc*4
// |------|------|---------|-------|------------|
// | argc | argv | argv[0] |  ...  | argv[argc] |
// |------|------|---------|-------|------------|
static inline void* argv_copy(uint32_t* dest, int argc, char** argv)
{
    char* temp;

    // Put argc
    *dest++ = argc;

    // Put argv address
    *dest = addr(dest + 1);
    dest++;

    // argv will always have size == argc + 1; argv[argc] must be 0
    temp = ptr(dest + argc + 1);

    for (int i = 0; i < argc; ++i)
    {
        // Put argv[i] address and copy content of argv[i]
        *dest++ = addr(temp);
        temp = strcpy(temp, argv[i]);
    }

    return temp;
}

static inline char** args_get(int* argc, const char* const argv[], size_t* args_size)
{
    size_t len;
    char** copied_argv;

    for (*argc = 0; argv[*argc]; ++*argc);

    *args_size =  sizeof(int) + sizeof(char**) + (*argc + 1) * sizeof(char*) + sizeof(int);

    copied_argv = fmalloc(sizeof(char*) * (*argc + 1));

    for (int i = 0; i < *argc; ++i)
    {
        len = strlen(argv[i]) + 1;
        copied_argv[i] = fmalloc(len);
        strcpy(copied_argv[i], argv[i]);
        *args_size += len;
    }

    copied_argv[*argc] = NULL;

    return copied_argv;
}

static inline void args_put(int argc, char** copied_argv)
{
    for (int i = 0; i < argc; ++i)
    {
        size_t len = strlen(copied_argv[i]) + 1;
        ffree(copied_argv[i], len);
    }

    ffree(copied_argv, sizeof(char*) * (argc + 1));
}

int do_exec(const char* pathname, const char* const argv[])
{
    int errno;
    int argc;
    void* data;
    void* entry;
    vm_area_t* stack_vma;
    vm_area_t* vm_areas = NULL;
    uint32_t brk;
    uint32_t user_stack;
    uint32_t* kernel_stack;
    size_t args_size;
    char** copied_argv;

    if ((errno = path_validate(pathname)))
    {
        return errno;
    }

    if ((errno = vm_verify(VERIFY_READ, argv, 4, process_current->mm->vm_areas)))
    {
        return errno;
    }

    copied_argv = args_get(&argc, argv, &args_size);

    log_debug(DEBUG_PROCESS, "%s, argc=%u, argv=%x", pathname, argc, argv);

    if ((errno = binary_image_get(pathname, &data)))
    {
        return errno;
    }

    if ((errno = read_elf(pathname, data, &vm_areas, &brk, &entry)))
    {
        return errno;
    }

    pgd_t* pgd = ptr(process_current->mm->pgd);
    log_debug(DEBUG_PROCESS, "pgd=%x", pgd);

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

    for (vm_area_t* temp = vm_areas; temp; temp = temp->next)
    {
        vm_apply(temp, pgd, VM_APPLY_REPLACE_PG);
    }

    pgd_reload();

    user_stack = vm_virt_end(stack_vma);

    process_current->mm->brk = brk;
    process_current->mm->args_start = user_stack - args_size;
    process_current->mm->args_end = user_stack;

    argv_copy(ptr(user_stack - args_size), argc, copied_argv);
    user_stack -= args_size;

    args_put(argc, copied_argv);

    ASSERT(*(int*)user_stack == argc);

    vm_add(&vm_areas, stack_vma);

    process_current->mm->vm_areas = vm_areas;
    process_current->type = USER_PROCESS;

    log_debug(DEBUG_PROCESS, "proc %d areas:", process_current->pid);
    vm_print(process_current->mm->vm_areas);

    arch_exec(entry, kernel_stack, user_stack);

    return 0;
}
