#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/signal.h>

/*===========================================================================*
 *                                sys_signal                                 *
 *===========================================================================*/
int sys_signal(int signum, sighandler_t handler) {

    if (!signum_exists(signum)) return -EINVAL;
    if (!signal_can_be_trapped(signum)) return -EINVAL;

    if  (!handler)
        process_current->signals->trapped &= !(1 << signum);
    else
        process_current->signals->trapped |= (1 << signum);
    process_current->signals->sighandler[signum] = handler;

    return 0;

}

/*===========================================================================*
 *                             default_sighandler                            *
 *===========================================================================*/
static void default_sighandler(struct process *p, int signum) {

    switch (signum) {
        case SIGCHLD:
        case SIGURG:
        case SIGWINCH:
        case SIGIO:
        case SIGPWR:
            /* Ignore */
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
            process_exit(p);
            break;
    }

}

/*===========================================================================*
 *                                  sys_kill                                 *
 *===========================================================================*/
int sys_kill(int pid, int signum) {

    struct process *p;

    (void)pid; (void)signum;

    if (!signum_exists(signum)) return -EINVAL;
    if (process_find(pid, &p)) return -ESRCH;
    if (!current_can_kill(p)) return -EPERM;

    if (p->signals->trapped & (1 << signum)) {
        process_wake(p);
        arch_process_execute(p, (unsigned int)p->signals->sighandler[signum]);
    } else
        default_sighandler(p, signum);

    return 0;

}

