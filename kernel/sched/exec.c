#include <kernel/exec.h>
#include <kernel/path.h>
#include <kernel/timer.h>
#include <kernel/module.h>
#include <kernel/process.h>
#include <kernel/backtrace.h>
#include <kernel/byteorder.h>

#include <kernel/api/elf.h>

static LIST_DECLARE(binfmts);

static int sheband_interp_read(file_t* file, string_t** interpreter);

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

#define INTERPRETER_LEN 32

static int sheband_interp_read(file_t* file, string_t** interpreter)
{
    int errno;
    string_it_t newline;

    if (unlikely(errno = string_read(file, 2, INTERPRETER_LEN, interpreter)))
    {
        return errno;
    }

    if (unlikely(!(newline = string_find(*interpreter, '\n'))))
    {
        log_debug(DEBUG_PROCESS, "shebang line too long");
        errno = -ENOEXEC;
        goto free_interpreter;
    }

    string_trunc(*interpreter, newline);

    return 0;

free_interpreter:
    string_free(*interpreter);
    return errno;
}

int aux_insert(int type, uintptr_t value, argvecs_t argvecs)
{
    aux_t* aux = fmalloc(sizeof(aux_t));

    if (unlikely(!aux))
    {
        return -ENOMEM;
    }

    aux->type = type;
    aux->value = value;
    list_init(&aux->list_entry);

    argvecs[AUXV].size += sizeof(aux_t);
    argvecs[AUXV].count++;

    list_add_tail(&aux->list_entry, &argvecs[AUXV].head);

    return 0;
}

int arg_insert(const char* string, const size_t len, argvec_t* argvec, int flag)
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

// auxs_put - free allocated auxv
//
// @argvec - vector to free
static void auxs_put(argvec_t* argvec)
{
    aux_t* aux;
    list_for_each_entry_safe(aux, &argvec->head, list_entry)
    {
        ffree(aux, sizeof(aux_t));
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
// | auxv       |
// +------------+ 16
// | argv[0]    |
// +------------+
// | ...        |
// +------------+ 16 + 4 * argc
// | argv[argc] |
// +------------+ 20 + 4 * argc
// | envp[0]    |
// +------------+
// | ...        |
// +------------+ 20 + 4 * (argc + n)
// | envp[n]    |
// +------------+ 24 + 4 * (argc + n)
// | argv str   |
// +------------+ variable
// | envp str   |
// +------------+ variable
// | auxv[0]    |
// +------------+
// | ...        |
// +------------+ variable
// | auxv[auxn] |
// +------------+ <- bottom
static void* argvecs_copy_to_user(uint32_t* dest, argvecs_t argvecs)
{
    char* strings;
    arg_t* arg;
    aux_t* aux;
    uintptr_t* temp;
    uintptr_t* vec[3];

    *dest++ = argvecs[ARGV].count;
    vec[ARGV] = dest++;
    vec[ENVP] = dest++;
    vec[AUXV] = dest++;

    // String table is located after all entries of argv and envp, which
    // both  have NULL entries at the end, thus "+1"s
    strings = ptr(dest + argvecs[ARGV].count + 1 + argvecs[ENVP].count + 1);

    for (int i = 0; i < 2; ++i)
    {
        // Set address of actual array to argv/envp
        *vec[i] = addr(dest);
        list_for_each_entry(arg, &argvecs[i].head, list_entry)
        {
            // Put argv/envp[i] address and copy content of argv/envp[i]
            *dest++ = addr(strings);
            strcpy(strings, arg->arg);
            strings += arg->len;
        }

        *dest++ = 0;
    }

    temp = ptr(align(addr(strings), sizeof(uintptr_t)));
    *vec[AUXV] = addr(temp);

    list_for_each_entry(aux, &argvecs[AUXV].head, list_entry)
    {
        *temp++ = aux->type;
        *temp++ = aux->value;
    }

    *temp++ = 0;
    *temp++ = 0;

    return temp;
}

static int binary_image_load(const char* pathname, binary_t* bin, argvecs_t argvecs)
{
    int errno;
    char buffer[2];
    binfmt_t* binfmt;
    void* data;
    string_t* tmp;
    scoped_file_t* file = NULL;
    scoped_string_t* interpreter = NULL;
    scoped_string_t* shebang_interp = NULL;

    for (;;)
    {
        data = NULL;
        file = NULL;
        tmp = NULL;

        if ((errno = do_open(&file, pathname, O_RDONLY, 0)))
        {
            return errno;
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
            if (unlikely(errno = sheband_interp_read(file, &shebang_interp)))
            {
                return errno;
            }

            // Insert current path as the argv[0] so the interpreter will know what to load
            if (unlikely(errno = argv_insert(pathname, strlen(pathname) + 1, argvecs, FRONT)))
            {
                return errno;
            }

            pathname = shebang_interp->data;
            do_close(file);
            continue;
        }

        if (unlikely(!(binfmt = binfmt_find(*buffer))))
        {
            log_debug(DEBUG_PROCESS, "invalid file");
            return -ENOEXEC;
        }

        if ((errno = binfmt->prepare(pathname, file, &tmp, &data)))
        {
            return errno;
        }

        if (tmp)
        {
            int fd;
            interpreter = tmp;
            log_debug(DEBUG_PROCESS, "%s: interpreter: %s", pathname, interpreter->data);

            errno = binfmt->load(file, bin, data, argvecs);

            pathname = interpreter->data;

            fd = file_fd_allocate(file);

            if (unlikely(errno = errno_get(fd)))
            {
                return errno;
            }

            if (unlikely(errno = aux_insert(AT_EXECFD, fd, argvecs)))
            {
                return errno;
            }

            binfmt->cleanup(data);
            continue;
        }

        if (interpreter)
        {
            errno = binfmt->interp_load(file, bin, data, argvecs);
        }
        else
        {
            errno = binfmt->load(file, bin, data, argvecs);
        }
        binfmt->cleanup(data);
        break;
    }

    return errno;
}

int do_exec(const char* pathname, const char* const argv[], const char* const envp[])
{
    binary_t* bin;
    char copied_path[PATH_MAX];
    int errno;
    uint32_t user_stack;
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

    if (!(bin = alloc(binary_t, memset(this, 0, sizeof(*this)))))
    {
        return -ENOMEM;
    }

    strcpy(copied_path, pathname);

    args_get(argv, true, &argvec[ARGV]);
    args_get(envp, false, &argvec[ENVP]);
    argvec[AUXV].size = sizeof(void*) + 2 * sizeof(uint32_t);

    if (aux_insert(AT_PAGESZ, PAGE_SIZE, argvec))
    {
        return -ENOMEM;
    }

    log_debug(DEBUG_PROCESS, "%s, argc=%u, envc=%u",
        pathname,
        argvec[ARGV].count,
        argvec[ENVP].count);

    if ((errno = binary_image_load(pathname, bin, argvec)))
    {
        return errno;
    }

    strncpy(p->name, copied_path, PROCESS_NAME_LEN);
    process_bin_exit(p);
    //process_ktimers_exit(p);
    p->bin = bin;
    bin->count = 1;

    if (unlikely(!bin->stack_vma))
    {
        do_kill(p, SIGSTKFLT);
        return -EFAULT;
    }

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
    auxs_put(&argvec[AUXV]);

    ASSERT(*(int*)user_stack == argvec[ARGV].count);

    p->type = USER_PROCESS;

    log_debug(DEBUG_PROCESS, "%s[%u] vma:", p->name, p->pid);
    vm_print(p->mm->vm_areas, DEBUG_PROCESS);

    // TODO: maybe I should alloc a new stack?
    arch_exec(bin->entry, p->mm->kernel_stack, user_stack);

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
        log_warning("process %u:%x without a stack; sending SIGSTKFLT", process_current->pid, process_current);
        do_kill(process_current, SIGSTKFLT);
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
