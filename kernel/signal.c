#include <kernel/mutex.h>
#include <kernel/signal.h>
#include <kernel/process.h>

const char* signame(int sig)
{
    switch (sig)
    {
        case 0: return "0";
        case SIGHUP: return "SIGHUP";
        case SIGINT: return "SIGINT";
        case SIGQUIT: return "SIGQUIT";
        case SIGILL: return "SIGILL";
        case SIGTRAP: return "SIGTRAP";
        case SIGABRT: return "SIGABRT";
        case SIGBUS: return "SIGBUS";
        case SIGFPE: return "SIGFPE";
        case SIGKILL: return "SIGKILL";
        case SIGUSR1: return "SIGUSR1";
        case SIGSEGV: return "SIGSEGV";
        case SIGUSR2: return "SIGUSR2";
        case SIGPIPE: return "SIGPIPE";
        case SIGALRM: return "SIGALRM";
        case SIGTERM: return "SIGTERM";
        case SIGSTKFLT: return "SIGSTKFLT";
        case SIGCHLD: return "SIGCHLD";
        case SIGCONT: return "SIGCONT";
        case SIGSTOP: return "SIGSTOP";
        case SIGTSTP: return "SIGTSTP";
        case SIGTTIN: return "SIGTTIN";
        case SIGTTOU: return "SIGTTOU";
        case SIGURG: return "SIGURG";
        case SIGXCPU: return "SIGXCPU";
        case SIGXFSZ: return "SIGXFSZ";
        case SIGVTALRM: return "SIGVTALRM";
        case SIGPROF: return "SIGPROF";
        case SIGWINCH: return "SIGWINCH";
        case SIGIO: return "SIGIO";
        case SIGPWR: return "SIGPWR";
        case SIGSYS: return "SIGSYS";
    }
    return "Unknown signal";
}

static void default_sighandler(process_t* p, int signum)
{
    switch (signum)
    {
        case SIGCHLD:
        case SIGURG:
        case SIGWINCH:
        case SIGIO:
        case SIGPWR:
            // Ignore
            break;
        case SIGCONT:
            process_wake(p);
            break;
        case SIGSTOP:
        case SIGTTIN:
        case SIGTTOU:
        case SIGTSTP:
            process_stop(p);
            break;
        default:
            p->exit_code = EXITCODE(0, signum);
            process_exit(p);
            break;
    }
}

static signal_t* signal_queued_find(process_t* proc, int signum)
{
    signal_t* s;
    list_for_each_entry(s, &proc->signals->queue, list_entry)
    {
        if (s->info.si_signo == signum)
        {
            return s;
        }
    }

    return NULL;
}

static void signal_drop(process_t* proc, int signum)
{
    signal_t* signal;
    scoped_mutex_lock(&proc->signals->lock);

    if (unlikely(!(signal = signal_queued_find(proc, signum))))
    {
        current_log_error("cannot find queued signal_t for %s for %s[%u]", signame(signum), proc->name, proc->pid);
        return;
    }

    list_del(&signal->list_entry);
    delete(signal);
}

static int signal_deliver(process_t* proc, int signum)
{
    if (proc->signals->ongoing & (1 << signum))
    {
        log_debug(DEBUG_SIGNAL, "ignoring %s for %s[%u]; already ongoing", signame(signum), proc->name, proc->pid);
        return 0;
    }

    if (proc->signals->trapped & (1 << signum))
    {
        if (proc->signals->sighandlers[signum].sa_handler == SIG_IGN)
        {
            log_debug(DEBUG_SIGNAL, "ignoring %s for %s[%u]", signame(signum), proc->name, proc->pid);
            signal_drop(proc, signum);
        }
        else
        {
            log_debug(DEBUG_SIGNAL, "scheduling custom handler for %s for %s[%u]", signame(signum), proc->name, proc->pid);

            proc->signals->ongoing |= (1 << signum);
            proc->need_signal = true;
        }

        if (proc->signals->sighandlers[signum].sa_flags & SA_RESETHAND)
        {
            proc->signals->trapped &= ~(1 << signum);
        }
    }
    else
    {
        log_debug(DEBUG_SIGNAL, "calling default handler for %s for %s[%u]", signame(signum), proc->name, proc->pid);
        default_sighandler(proc, signum);
        signal_drop(proc, signum);
    }

    return 0;
}

int signal_run(process_t* proc)
{
    uint32_t* pending = &proc->signals->pending;
    int signals = 0;

    for (int signum = 0; *pending; ++signum)
    {
        if (!(*pending & (1 << signum)))
        {
            continue;
        }

        *pending &= ~(1 << signum);
        ++signals;
        signal_deliver(proc, signum);
    }

    return signals;
}

static signal_t* signal_allocate(int signum)
{
    signal_t* signal = zalloc(signal_t);

    if (unlikely(!signal))
    {
        return NULL;
    }

    list_init(&signal->list_entry);
    signal->info.si_signo = signum;

    return signal;
}

int do_kill(process_t* proc, siginfo_t* siginfo)
{
    int signum = siginfo->si_signo;

    log_debug(DEBUG_SIGNAL, "%s to %s[%u]", signame(signum), proc->name, proc->pid);

    if (!signum_exists(signum))
    {
        return -EINVAL;
    }

    if (!current_can_kill(proc))
    {
        return -EPERM;
    }

    if (unlikely(proc->stat == PROCESS_ZOMBIE))
    {
        return 0;
    }

    if ((proc->signals->pending | proc->signals->ongoing) & (1 << signum))
    {
        log_debug(DEBUG_SIGNAL, "ignoring %s for %s[%u]; already ongoing/pending", signame(signum), proc->name, proc->pid);
        return 0;
    }

    signal_t* signal = signal_allocate(signum);

    if (unlikely(!signal))
    {
        return -ENOMEM;
    }

    memcpy(&signal->info, siginfo, sizeof(*siginfo));

    {
        scoped_mutex_lock(&proc->signals->lock);
        list_add(&signal->list_entry, &proc->signals->queue);
    }

    if (proc->stat == PROCESS_WAITING)
    {
        process_wake(proc);
        proc->signals->pending |= 1 << signum;
        return 0;
    }

    return signal_deliver(proc, signum);
}

int sys_kill(int pid, int signum)
{
    process_t* p;

    if (process_find(pid, &p))
    {
        log_debug(DEBUG_SIGNAL, "no process with pid: %d", pid);
        return -ESRCH;
    }

    if (unlikely(p->type == KERNEL_PROCESS))
    {
        log_debug(DEBUG_SIGNAL, "cannot send signal to kernel process %d", pid);
        return -EPERM;
    }

    log_debug(DEBUG_SIGNAL, "sending %s to pid %d", signame(signum), pid);

    siginfo_t siginfo = {
        .si_code = SI_USER,
        .si_pid = process_current->pid,
        .si_uid = process_current->uid,
        .si_signo = signum,
    };

    return do_kill(p, &siginfo);
}

int sys_sigaction(int signum, const sigaction_t* act, sigaction_t* oact)
{
    if (!signum_exists(signum))
    {
        return -EINVAL;
    }

    if (!signal_can_be_trapped(signum))
    {
        return -EINVAL;
    }

    if (oact)
    {
        if (current_vm_verify(VERIFY_WRITE, oact))
        {
            return -EFAULT;
        }

        memcpy(oact, &process_current->signals->sighandlers[signum], sizeof(*oact));
    }

    if (act)
    {
        if (current_vm_verify(VERIFY_READ, act))
        {
            return -EFAULT;
        }

        memcpy(&process_current->signals->sighandlers[signum], act, sizeof(*act));

        if (act->sa_handler == SIG_DFL)
        {
            process_current->signals->trapped &= ~(1 << signum);
        }
        else
        {
            process_current->signals->trapped |= (1 << signum);
        }
    }

    return 0;
}
