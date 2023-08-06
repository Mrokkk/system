#include <kernel/signal.h>
#include <kernel/process.h>

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

    log_debug(DEBUG_SIGNAL, "%u:%x set handler for %u",
        process_current->pid,
        process_current,
        signum);

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

void signal_run(struct process* proc)
{
    uint32_t* pending = &proc->signals->pending;

    for (int signum = 0; *pending; ++signum)
    {
        if (!(*pending & (1 << signum)))
        {
            continue;
        }

        log_debug(DEBUG_SIGNAL, "sending %u to %u", signum, proc->pid);
        *pending &= ~(1 << signum);
        signal_deliver(proc, signum);
    }
}

int signal_deliver(struct process* proc, int signum)
{
    if (proc->signals->trapped & (1 << signum))
    {
        log_debug(DEBUG_SIGNAL, "calling custom handler for %u in %u", signum, proc->pid);
        arch_process_execute_sighan(
            proc,
            addr(proc->signals->sighandler[signum]),
            addr(proc->signals->sigrestorer));
    }
    else
    {
        log_debug(DEBUG_SIGNAL, "calling default handler for %u in %u", signum, proc->pid);
        default_sighandler(proc, signum);
    }

    return 0;
}

int do_kill(struct process* proc, int signum)
{
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

    log_debug(DEBUG_SIGNAL, "sending %d to pid %d", signum, pid);

    return do_kill(p, signum);
}

int sys_sigaction(int, const struct sigaction* act, struct sigaction*)
{
    if (!act->sa_restorer)
    {
        return -EINVAL;
    }
    process_current->signals->sigrestorer = act->sa_restorer;
    return 0;
}
