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

int sys_signal(int signum, sighandler_t handler)
{
    if (!signum_exists(signum))
    {
        return -EINVAL;
    }

    if (!signal_can_be_trapped(signum))
    {
        return -EINVAL;
    }

    if (!handler)
    {
        process_current->signals->trapped &= ~(1 << signum);
    }
    else
    {
        process_current->signals->trapped |= (1 << signum);
    }

    log_debug(DEBUG_SIGNAL, "%u:%x set handler for %s: %x",
        process_current->pid,
        process_current,
        signame(signum),
        handler);

    process_current->signals->sighandler[signum] = handler;

    return 0;
}

static void default_sighandler(struct process* p, int signum)
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

int signal_run(struct process* proc)
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

int signal_deliver(struct process* proc, int signum)
{
    if (proc->signals->ongoing & (1 << signum))
    {
        log_debug(DEBUG_SIGNAL, "ignoring %s in %u; already ongoing", signame(signum), proc->pid);
        return 0;
    }

    if (proc->signals->trapped & (1 << signum))
    {
        if (proc->signals->sighandler[signum] == SIG_IGN)
        {
            log_debug(DEBUG_SIGNAL, "ignoring %s in %u", signame(signum), proc->pid);
            return 0;
        }

        log_debug(DEBUG_SIGNAL, "scheduling custom handler for %s in %u", signame(signum), proc->pid);

        proc->signals->ongoing |= (1 << signum);
        proc->need_signal = true;
    }
    else
    {
        log_debug(DEBUG_SIGNAL, "calling default handler for %s in %u", signame(signum), proc->pid);
        default_sighandler(proc, signum);
    }

    return 0;
}

int do_kill(struct process* proc, int signum)
{
    log_debug(DEBUG_SIGNAL, "%s to %s[%u]", signame(signum), proc->name, proc->pid);

    if (!signum_exists(signum))
    {
        return -EINVAL;
    }

    if (!current_can_kill(proc))
    {
        return -EPERM;
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
    struct process* p;

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

    return do_kill(p, signum);
}

int sys_sigaction(int sig, const struct sigaction* act, struct sigaction* oact)
{
    if (sig == 0 && act && act->sa_restorer)
    {
        process_current->signals->sigrestorer = act->sa_restorer;
        return 0;
    }

    if (oact)
    {
        oact->sa_flags = 0;
        oact->sa_mask = 0;
        oact->sa_sigaction = NULL;
        oact->sa_restorer = process_current->signals->sigrestorer;
        oact->sa_handler = process_current->signals->sighandler[sig];
    }

    if (act)
    {
        return sys_signal(sig, act->sa_handler);
    }

    return 0;
}
