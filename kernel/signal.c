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

    if (proc->signals->trapped & (1 << signum))
    {
        log_debug("calling handler");
        process_wake(proc);
        arch_process_execute_sighan(proc, addr(proc->signals->sighandler[signum]));
    }
    else
    {
        log_debug("calling default handler");
        default_sighandler(proc, signum);
    }

    return 0;
}

int sys_kill(int pid, int signum)
{
    struct process* p;

    if (process_find(pid, &p))
    {
        return -ESRCH;
    }

    return do_kill(p, signum);
}
