#include <kernel/exec.h>
#include <kernel/path.h>
#include <kernel/timer.h>
#include <kernel/module.h>
#include <kernel/process.h>
#include <kernel/vm_print.h>
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

aux_t* aux_insert(int type, uintptr_t value, argvecs_t argvecs)
{
    aux_t* aux = fmalloc(sizeof(aux_t));

    if (unlikely(!aux))
    {
        return NULL;
    }

    aux->ptr = NULL;
    aux->type = type;
    aux->value = value;
    list_init(&aux->list_entry);

    argvecs[AUXV].size += sizeof(aux_t);
    argvecs[AUXV].count++;

    list_add_tail(&aux->list_entry, &argvecs[AUXV].head);

    return aux;
}

aux_t* aux_data_insert(int type, const void* data_ptr, size_t size, argvecs_t argvecs)
{
    aux_t* aux = aux_insert(type, 0, argvecs);

    if (unlikely(!aux))
    {
        return NULL;
    }

    aux_data_t* data = fmalloc(sizeof(*data));

    if (unlikely(!data))
    {
        return NULL;
    }

    data->data = data_ptr;
    data->size = size;
    data->aux = aux;
    list_init(&data->list_entry);

    argvecs[AUX_DATA].size += data->size;
    argvecs[AUX_DATA].count++;

    list_add_tail(&data->list_entry, &argvecs[AUX_DATA].head);

    return aux;
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
static void args_get(const char* const vec[], bool has_count, argvec_t* argvec)
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
}

static void args_put(argvec_t* argvec)
{
    arg_t* arg;
    list_for_each_entry_safe(arg, &argvec->head, list_entry)
    {
        ffree(arg, sizeof(arg_t) + (arg->arg ? arg->len : 0));
    }
}

static void auxs_put(argvec_t* argvec)
{
    aux_t* aux;
    list_for_each_entry_safe(aux, &argvec->head, list_entry)
    {
        ffree(aux, sizeof(aux_t));
    }
}

static void aux_data_put(argvec_t* argvec)
{
    aux_data_t* aux;
    list_for_each_entry_safe(aux, &argvec->head, list_entry)
    {
        ffree(aux, sizeof(*aux));
    }
}

static void argvecs_put(argvecs_t argvecs)
{
    args_put(&argvecs[ARGV]);
    args_put(&argvecs[ENVP]);
    auxs_put(&argvecs[AUXV]);
    aux_data_put(&argvecs[AUX_DATA]);
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
// +------------+ variable
// | aux data   |
// +------------+ <- bottom
static void argvecs_copy_to_user(uint32_t* dest, argvecs_t argvecs)
{
    char* strings;
    arg_t* arg;
    aux_t* aux;
    aux_data_t* data;
    uintptr_t* temp;
    uintptr_t* vec[3];

    *dest++ = argvecs[ARGV].count;
    vec[ARGV] = dest++;
    vec[ENVP] = dest++;
    vec[AUXV] = dest++;

    // String table is located after all entries of argv and envp, which
    // both have NULL entries at the end, thus "+1"s
    strings = ptr(dest + argvecs[ARGV].count + 1 + argvecs[ENVP].count + 1);

    for (int i = 0; i < 2; ++i)
    {
        // Set address of actual array to argv/envp
        *vec[i] = addr(dest);
        argvecs[i].data = strings;
        list_for_each_entry(arg, &argvecs[i].head, list_entry)
        {
            // Put argv/envp[i] address and copy content of argv/envp[i]
            *dest++ = addr(strings);
            strcpy(strings, arg->arg);
            strings += arg->len;
            argvecs[i].data_size += arg->len;
        }

        *dest++ = 0;
    }

    temp = ptr(align(addr(strings), sizeof(uintptr_t)));
    *vec[AUXV] = addr(temp);

    list_for_each_entry(aux, &argvecs[AUXV].head, list_entry)
    {
        *temp++ = aux->type;
        aux->ptr = temp;
        *temp++ = aux->value;
    }

    *temp++ = 0;
    *temp++ = 0;

    list_for_each_entry(data, &argvecs[AUX_DATA].head, list_entry)
    {
        *data->aux->ptr = addr(temp);
        memcpy(temp, data->data, data->size);
    }
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

        if (unlikely(!(file->dentry->inode->mode & S_IXUGO)))
        {
            return -EACCES;
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

            if (unlikely(!aux_insert(AT_EXECFD, fd, argvecs)))
            {
                return -ENOMEM;
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
    char copied_path[PATH_MAX];
    int errno;
    uint32_t user_stack;
    aux_t* execfn;
    arg_t* argv0;
    process_t* p = process_current;
    ARGVECS_DECLARE(argvec);
    binary_t bin = {};

    vm_area_t* brk_vma;
    vm_area_t* stack_vma;
    vm_area_t* old_vmas = p->mm->vm_areas;
    pgd_t* old_pgd = p->mm->pgd;
    pgd_t* new_pgd = pgd_alloc();

    if (unlikely(!new_pgd))
    {
        return -ENOMEM;
    }

    strlcpy(copied_path, pathname, sizeof(copied_path));
    args_get(argv, true, &argvec[ARGV]);
    args_get(envp, false, &argvec[ENVP]);

    pgd_load(new_pgd);

    p->mm->pgd = new_pgd;
    p->mm->vm_areas = NULL;

    argv0 = list_next_entry(&argvec[ARGV].head, arg_t, list_entry);

    argvec[AUXV].size = sizeof(void*) + 2 * sizeof(uint32_t);

    execfn = aux_data_insert(AT_EXECFN, copied_path, strlen(copied_path) + 1, argvec);

    if (!aux_insert(AT_PAGESZ, PAGE_SIZE, argvec) || !execfn)
    {
        errno = -ENOMEM;
        goto restore;
    }

    log_debug(DEBUG_PROCESS, "%s, argc=%u, envc=%u",
        copied_path,
        argvec[ARGV].count,
        argvec[ENVP].count);

    if ((errno = binary_image_load(copied_path, &bin, argvec)))
    {
        goto restore;
    }

    strlcpy(p->name, argv0->arg, PROCESS_NAME_LEN);
    //process_ktimers_exit(p);

    brk_vma = vm_create(bin.brk, 0, VM_READ | VM_WRITE | VM_TYPE(VM_TYPE_HEAP));
    stack_vma = vm_create(USER_STACK_VIRT_ADDRESS, USER_STACK_SIZE, VM_WRITE | VM_READ | VM_TYPE(VM_TYPE_STACK));

    if (unlikely(!brk_vma || !stack_vma))
    {
        errno = -ENOMEM;
        goto restore;
    }

    if (unlikely(
        (errno = vm_add(&p->mm->vm_areas, brk_vma)) ||
        (errno = vm_add(&p->mm->vm_areas, stack_vma))))
    {
        errno = -ENOMEM;
        goto restore;
    }

    user_stack = stack_vma->end;

    user_stack -= ARGVECS_SIZE(argvec);
    argvecs_copy_to_user(ptr(user_stack), argvec);

    p->type = USER_PROCESS;

    p->signals->trapped = 0;
    p->signals->ongoing = 0;
    p->signals->pending = 0;

    p->mm->brk = bin.brk;
    p->mm->brk_vma = brk_vma;
    p->mm->code_start = bin.code_start;
    p->mm->code_end = bin.code_end;

    p->mm->env_start = addr(argvec[ENVP].data);
    p->mm->env_end = p->mm->env_start + argvec[ENVP].data_size;

    p->mm->args_start = addr(argvec[ARGV].data);
    p->mm->args_end = p->mm->args_start + argvec[ARGV].data_size;

    p->mm->stack_start = stack_vma->start;
    p->mm->stack_end = stack_vma->end;

    p->mm->syscalls_start = 0;
    p->mm->syscalls_end = 0;

    argvecs_put(argvec);

    ASSERT(*(int*)user_stack == argvec[ARGV].count);

    if (DEBUG_PROCESS)
    {
        process_vm_areas_indent_log(KERN_DEBUG, p, INDENT_LVL_1);
    }

    vm_free(old_vmas, old_pgd);
    page_free(old_pgd);

    arch_exec(bin.entry, p->kernel_stack, user_stack);

    ASSERT_NOT_REACHED();

restore:
    argvecs_put(argvec);
    vm_free(p->mm->vm_areas, p->mm->pgd);

    p->mm->vm_areas = old_vmas;
    p->mm->pgd = old_pgd;
    pgd_load(old_pgd);
    if (new_pgd)
    {
        page_free(new_pgd);
    }
    return errno;
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
