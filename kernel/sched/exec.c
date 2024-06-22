#include <kernel/elf.h>
#include <kernel/path.h>
#include <kernel/timer.h>
#include <kernel/module.h>
#include <kernel/process.h>
#include <kernel/backtrace.h>

LIST_DECLARE(libraries);

static int binary_image_load(const char* pathname, binary_t* bin)
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

// Memory layout of stack:
// +------------+ 0 <- top
// | argc       |
// +------------+ 4
// | argv       |
// +------------+ 8
// | envp       |
// +------------+ 12
// | argv[0]    |
// +------------+
// | ...        |
// +------------+ 12 + 4 * argc
// | argv[argc] |
// +------------+ 16 + 4 * argc
// | envp[0]    |
// +------------+
// | ...        |
// +------------+ 16 + 4 * (argc + n)
// | envp[n]    |
// +------------+ 20 + 4 * (argc + n)
// | argv str   |
// +------------+ variable
// | envp str   |
// +------------+ <- bottom
static void* argv_envp_copy(uint32_t* dest, int argc, char** argv, int envc, char** envp)
{
    char* temp;
    size_t len;

    // Put argc
    *dest++ = argc;

    // Put argv address, dest + 2, because next will be envp
    *dest = addr(dest + 2);
    dest++;

    // Put envp address, argc + 2, because there will be argc + 1
    // entries before envp[0]
    *dest = addr(dest + argc + 2);
    dest++;

    // argv will always have size == argc + 1; argv[argc] must be 0
    temp = ptr(dest + argc + 2 + envc);

    for (int i = 0; i < argc; ++i)
    {
        // Put argv[i] address and copy content of argv[i]
        *dest++ = addr(temp);
        len = strlen(argv[i]);
        strcpy(temp, argv[i]);
        temp += len + 1;
    }

    *dest++ = 0;

    for (int i = 0; i < envc; ++i)
    {
        // Put envp[i] address and copy content of envp[i]
        *dest++ = addr(temp);
        len = strlen(envp[i]);
        strcpy(temp, envp[i]);
        temp += len + 1;
    }

    *dest++ = 0;

    return temp;
}

static char** args_get(const char* const argv[], bool has_count, int* argc, size_t* args_size)
{
    size_t len;
    char** copied_argv;
    int count = 0;

    for (count = 0; argv[count]; ++count);

    *args_size =  (has_count ? sizeof(int) : 0) + sizeof(char**) + (count + 1) * sizeof(char*);

    copied_argv = fmalloc(sizeof(char*) * (count + 1));

    for (int i = 0; i < count; ++i)
    {
        len = strlen(argv[i]) + 1;
        copied_argv[i] = fmalloc(len);
        strcpy(copied_argv[i], argv[i]);
        *args_size += len;
    }

    copied_argv[count] = NULL;
    *argc = count;

    return copied_argv;
}

static void args_put(int argc, char** copied_argv)
{
    for (int i = 0; i < argc; ++i)
    {
        size_t len = strlen(copied_argv[i]) + 1;
        ffree(copied_argv[i], len);
    }

    ffree(copied_argv, sizeof(char*) * (argc + 1));
}

int do_exec(const char* pathname, const char* const argv[], const char* const envp[])
{
    binary_t* bin;
    char copied_path[PATH_MAX];
    int errno, argc, envc;
    uint32_t user_stack;
    uint32_t* kernel_stack;
    size_t args_size, env_size;
    char** copied_argv;
    char** copied_envp;
    struct process* p = process_current;

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
    copied_argv = args_get(argv, true, &argc, &args_size);
    copied_envp = args_get(envp, false, &envc, &env_size);

    log_debug(DEBUG_PROCESS, "%s, argc=%u, argv=%x, envc=%u, envp=%x",
        pathname,
        argc,
        argv,
        envc,
        envp);

    kernel_stack = p->mm->kernel_stack; // FIXME: maybe I should alloc a new stack?
                                        // Maybe I should also move this after binary_image_load?

    if ((errno = binary_image_load(pathname, bin)))
    {
        return errno;
    }

    strncpy(p->name, copied_path, PROCESS_NAME_LEN);
    process_bin_exit(p);
    //process_ktimers_exit(p);
    p->bin = bin;
    bin->count = 1;

    pgd_reload();

    user_stack = bin->stack_vma->end;

    p->mm->brk = bin->brk;
    p->mm->args_start = user_stack - args_size;
    p->mm->args_end = user_stack;
    p->signals->trapped = 0;

    user_stack -= args_size + env_size;
    argv_envp_copy(ptr(user_stack), argc, copied_argv, envc, copied_envp);

    args_put(argc, copied_argv);
    args_put(envc, copied_envp);

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
