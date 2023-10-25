#include <kernel/elf.h>
#include <kernel/path.h>
#include <kernel/module.h>
#include <kernel/process.h>

LIST_DECLARE(libraries);

static inline int binary_image_load(const char* pathname, binary_t* bin)
{
    int errno;
    char buffer[4];
    scoped_file_t* file = NULL;

    if ((errno = do_open(&file, pathname, O_RDONLY, 0)))
    {
        return errno;
    }

    if (unlikely(!file))
    {
        log_error("null file returned by open; pathname = %s", pathname);
        return -ENOENT;
    }

    if (unlikely(!file->ops || !file->ops->mmap || !file->ops->read))
    {
        return -ENOSYS;
    }

    if (unlikely(errno = do_read(file, 0, buffer, 4)))
    {
        return errno;
    }

    switch (*buffer)
    {
        case ELFMAG0:
            errno = elf_load(pathname, file, bin);
            break;
        default:
            log_debug(DEBUG_PROCESS, "invalid file");
            return -ENOEXEC;
    }

    bin->inode = file->inode;

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
    size_t len;

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
        len = strlen(argv[i]);
        strcpy(temp, argv[i]);
        temp += len + 1;
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
    binary_t* bin;
    char copied_path[256];
    int errno, argc;
    uint32_t user_stack;
    uint32_t* kernel_stack;
    size_t args_size;
    char** copied_argv;
    struct process* p = process_current;
    pgd_t* pgd = ptr(p->mm->pgd);

    if ((errno = path_validate(pathname)))
    {
        return errno;
    }

    if ((errno = vm_verify(VERIFY_READ, argv, 4, p->mm->vm_areas)))
    {
        return errno;
    }

    if (!(bin = alloc(binary_t)))
    {
        return -ENOMEM;
    }

    strcpy(copied_path, pathname);
    copied_argv = args_get(&argc, argv, &args_size);

    log_debug(DEBUG_PROCESS, "%s, argc=%u, argv=%x, pgd=%x", pathname, argc, argv, pgd);

    kernel_stack = p->mm->kernel_stack; // FIXME: maybe I should alloc a new stack?

    if ((errno = binary_image_load(pathname, bin)))
    {
        return errno;
    }

    strncpy(p->name, copied_path, PROCESS_NAME_LEN);
    process_bin_exit(p);
    p->bin = bin;
    bin->count = 1;

    pgd_reload();

    user_stack = bin->stack_vma->end;

    p->mm->brk = bin->brk;
    p->mm->args_start = user_stack - args_size;
    p->mm->args_end = user_stack;
    p->signals->trapped = 0;

    argv_copy(ptr(user_stack - args_size), argc, copied_argv);
    user_stack -= args_size;

    args_put(argc, copied_argv);

    ASSERT(*(int*)user_stack == argc);

    p->type = USER_PROCESS;

    log_debug(DEBUG_PROCESS, "%s[%u] vma:", p->name, p->pid);
    vm_print(p->mm->vm_areas, DEBUG_PROCESS);

    arch_exec(bin->entry, kernel_stack, user_stack);

    return 0;
}

vm_area_t* exec_prepare_initial_vma()
{
    vm_area_t* prev;
    vm_area_t* temp;
    vm_area_t* stack_vma = process_stack_vm_area(process_current);

    if (likely(stack_vma))
    {
        for (temp = process_current->mm->vm_areas; temp;)
        {
            prev = temp;
            if (temp != stack_vma)
            {
                vm_unmap(temp, process_current->mm->pgd);
                temp = temp->next;
                delete(prev);
            }
            else
            {
                temp = temp->next;
                prev->next = NULL;
            }
        }

        stack_vma->next = NULL;
        stack_vma->prev = NULL;
        process_current->mm->vm_areas = NULL;
    }
    else if (process_is_kernel(process_current))
    {
        stack_vma = stack_create(USER_STACK_VIRT_ADDRESS, process_current->mm->pgd);
        process_current->mm->stack_start = stack_vma->start;
        process_current->mm->stack_end = stack_vma->end;
    }
    else
    {
        log_warning("process %u:%x without a stack; sending SIGSEGV", process_current->pid, process_current);
        do_kill(process_current, SIGSEGV);
    }

    vm_add(&process_current->mm->vm_areas, stack_vma);

    return stack_vma;
}
