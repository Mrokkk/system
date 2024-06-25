#include <kernel/exec.h>
#include <kernel/path.h>
#include <kernel/timer.h>
#include <kernel/module.h>
#include <kernel/process.h>
#include <kernel/backtrace.h>
#include <kernel/byteorder.h>

static LIST_DECLARE(binfmts);

static int shebang_prepare(const char* name, file_t* file, interp_t** interpreter, void** data);
static void shebang_cleanup(void* data);

static binfmt_t* binfmt_find(int signature)
{
    binfmt_t* binfmt;

    list_for_each_entry(binfmt, &binfmts, list_entry)
    {
        if (signature == binfmt->signature)
        {
            return binfmt;
        }
    }

    return NULL;
}

static binfmt_t shebang_fmt = {
    .prepare = &shebang_prepare,
    .cleanup = &shebang_cleanup,
};

#define INTERPRETER_LEN 17

static int shebang_prepare(const char*, file_t* file, interp_t** interpreter, void**)
{
    int errno;
    char* newline;

    if (unlikely(!(*interpreter = fmalloc(INTERPRETER_LEN + sizeof(interp_t)))))
    {
        log_debug(DEBUG_PROCESS, "cannot allocate memory for interpreter");
        return -ENOMEM;
    }

    (*interpreter)->data = shift_as(char*, *interpreter, sizeof(interp_t));
    (*interpreter)->len = INTERPRETER_LEN;

    if (unlikely(errno = do_read(file, 2, (*interpreter)->data, INTERPRETER_LEN)))
    {
        log_debug(DEBUG_PROCESS, "failed to read line: %d", errno);
        goto free_interpreter;
    }

    (*interpreter)->data[INTERPRETER_LEN - 1] = 0;

    if (unlikely(!(newline = strchr((*interpreter)->data, '\n'))))
    {
        log_debug(DEBUG_PROCESS, "line too long");
        errno = -ENOEXEC;
        goto free_interpreter;
    }

    *newline = 0;

    return 0;

free_interpreter:
    ffree(*interpreter, sizeof(interp_t) + INTERPRETER_LEN);
    return errno;
}

static void shebang_cleanup(void*)
{
}

#define FRONT 1
#define TAIL  0

// arg_insert - allocate and insert new arg at front/tail of argvec
//
// @string - string arg
// @len - length of arg including '\0'
// @argvec - argvec to which new arg is inserted
// @flag - whetner new entry should be added at front or tail
static int arg_insert(const char* string, const size_t len, argvec_t* argvec, int flag)
{
    arg_t* arg = fmalloc(len + sizeof(arg_t));

    if (unlikely(!arg))
    {
        return -ENOMEM;
    }

    arg->len = len;
    arg->arg = shift_as(char*, arg, sizeof(arg_t));

    strcpy(arg->arg, string);
    list_init(&arg->list_entry);

    argvec->size += sizeof(char*) + len;
    argvec->count++;

    flag == FRONT
        ? list_add(&arg->list_entry, &argvec->head)
        : list_add_tail(&arg->list_entry, &argvec->head);

    return 0;
}

// args_get - allocate and copy arg vector (argv, envp)
//
// @vec - original vector to copy from
// @has_count - true for argv, false for envp
// @argvec - output vector
static int args_get(const char* const vec[], bool has_count, argvec_t* argvec)
{
    int count = 0;

    for (count = 0; vec[count]; ++count);

    // Size of char** (e.g. argv) plus optional count (e.g. argc)
    argvec->size = (has_count ? sizeof(int) : 0) + sizeof(char**);

    for (int i = 0; i < count; ++i)
    {
        arg_insert(vec[i], strlen(vec[i]) + 1, argvec, TAIL);
    }

    // Extend size for the NULL entry
    argvec->size += sizeof(char*);

    return 0;
}

// args_put - free allocated argvec
//
// @argvec - vector to free
static void args_put(argvec_t* argvec)
{
    arg_t* arg;
    list_for_each_entry_safe(arg, &argvec->head, list_entry)
    {
        ffree(arg, sizeof(arg_t) + (arg->arg ? arg->len : 0));
    }
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
static void* argvecs_copy_to_user(uint32_t* dest, argvecs_t argvecs)
{
    char* temp;
    arg_t* arg;

    // Put argc
    *dest++ = argvecs[ARGV].count;

    // Put argv address, dest + 2, because next will be envp
    *dest = addr(dest + 2);
    dest++;

    // Put envp address, argc + 2, because there will be argc + 1
    // entries before envp[0]
    *dest = addr(dest + argvecs[ARGV].count + 2);
    dest++;

    // argv will always have size == argc + 1; argv[argc] must be 0
    temp = ptr(dest + argvecs[ARGV].count + 2 + argvecs[ENVP].count);

    for (int i = 0; i < 2; ++i)
    {
        list_for_each_entry(arg, &argvecs[i].head, list_entry)
        {
            // Put argv/envp[i] address and copy content of argv/envp[i]
            *dest++ = addr(temp);
            strcpy(temp, arg->arg);
            temp += arg->len;
        }

        *dest++ = 0;
    }

    return temp;
}

static int binary_image_load(const char* pathname, binary_t* bin, argvecs_t argvecs)
{
    int errno;
    char buffer[2];
    binfmt_t* binfmt;
    void* data;
    interp_t* tmp;
    interp_t* interpreter = NULL;
    scoped_file_t* file = NULL;

    for (;;)
    {
        data = NULL;
        file = NULL;
        tmp = NULL;

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

        if (unlikely(errno = do_read(file, 0, buffer, 2)))
        {
            return errno;
        }

        if (*(uint16_t*)buffer == U16('#', '!'))
        {
            binfmt = &shebang_fmt;
        }
        else
        {
            if (unlikely(!(binfmt = binfmt_find(*buffer))))
            {
                log_debug(DEBUG_PROCESS, "invalid file");
                return -ENOEXEC;
            }
        }

        if ((errno = binfmt->prepare(pathname, file, &tmp, &data)))
        {
            return errno;
        }

        if (interpreter && tmp)
        {
            log_info("%s: no support for nested interpreters", pathname);
            binfmt->cleanup(data);
            return -ENOEXEC;
        }
        else if (tmp)
        {
            interpreter = tmp;
            log_debug(DEBUG_PROCESS, "%s: interpreter: %s", pathname, interpreter->data);

            // Insert current path as the argv[0] so the interpreter will know what to load
            if (unlikely(errno = arg_insert(pathname, strlen(pathname) + 1, &argvecs[ARGV], FRONT)))
            {
                return errno;
            }

            pathname = interpreter->data;
            do_close(file);
            binfmt->cleanup(data);
            continue;
        }

        errno = binfmt->load(file, bin, data);
        binfmt->cleanup(data);
        break;
    }

    if (interpreter)
    {
        ffree(interpreter, interpreter->len + sizeof(*interpreter));
    }

    return errno;
}

int do_exec(const char* pathname, const char* const argv[], const char* const envp[])
{
    binary_t* bin;
    char copied_path[PATH_MAX];
    int errno;
    uint32_t user_stack;
    uint32_t* kernel_stack;
    struct process* p = process_current;
    ARGVECS_DECLARE(argvec);

    if ((errno = path_validate(pathname)))
    {
        return errno;
    }

    // FIXME: validate properly instead of checking only 4 bytes
    if ((errno = vm_verify_array(VERIFY_READ, argv, 4, p->mm->vm_areas)))
    {
        return errno;
    }

    if (!(bin = alloc(binary_t)))
    {
        return -ENOMEM;
    }

    strcpy(copied_path, pathname);

    args_get(argv, true, &argvec[ARGV]);
    args_get(envp, false, &argvec[ENVP]);

    log_debug(DEBUG_PROCESS, "%s, argc=%u, envc=%u",
        pathname,
        argvec[ARGV].count,
        argvec[ENVP].count);

    if ((errno = binary_image_load(pathname, bin, argvec)))
    {
        return errno;
    }

    kernel_stack = p->mm->kernel_stack; // FIXME: maybe I should alloc a new stack?

    strncpy(p->name, copied_path, PROCESS_NAME_LEN);
    process_bin_exit(p);
    //process_ktimers_exit(p);
    p->bin = bin;
    bin->count = 1;

    pgd_reload();

    user_stack = bin->stack_vma->end;

    p->mm->brk = bin->brk;
    p->mm->args_start = user_stack - argvec[ARGV].size;
    p->mm->args_end = user_stack;

    p->signals->trapped = 0;
    p->signals->ongoing = 0;
    p->signals->pending = 0;

    user_stack -= ARGVECS_SIZE(argvec);
    argvecs_copy_to_user(ptr(user_stack), argvec);

    args_put(&argvec[ARGV]);
    args_put(&argvec[ENVP]);

    ASSERT(*(int*)user_stack == argvec[ARGV].count);

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

int binfmt_register(binfmt_t* binfmt)
{
    binfmt_t* tmp;

    list_for_each_entry(tmp, &binfmts, list_entry)
    {
        if (!strcmp(tmp->name, binfmt->name))
        {
            return -EBUSY;
        }
    }

    list_init(&binfmt->list_entry);
    list_add_tail(&binfmt->list_entry, &binfmts);

    return 0;
}
