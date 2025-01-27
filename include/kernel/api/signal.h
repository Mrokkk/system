#pragma once

#include <kernel/api/types.h>

#define SIGHUP       1
#define SIGINT       2
#define SIGQUIT      3
#define SIGILL       4
#define SIGTRAP      5
#define SIGABRT      6
#define SIGIOT       6
#define SIGBUS       7
#define SIGFPE       8
#define SIGKILL      9
#define SIGUSR1     10
#define SIGSEGV     11
#define SIGUSR2     12
#define SIGPIPE     13
#define SIGALRM     14
#define SIGTERM     15
#define SIGSTKFLT   16
#define SIGCHLD     17
#define SIGCONT     18
#define SIGSTOP     19
#define SIGTSTP     20
#define SIGTTIN     21
#define SIGTTOU     22
#define SIGURG      23
#define SIGXCPU     24
#define SIGXFSZ     25
#define SIGVTALRM   26
#define SIGPROF     27
#define SIGWINCH    28
#define SIGIO       29
#define SIGPOLL     SIGIO
#define SIGPWR      30
#define SIGSYS      31
#define SIGUNUSED   31

#define NSIGNALS    32

#define SIG_ERR     ((void (*)(int))-1)
#define SIG_DFL     ((void (*)(int))0)
#define SIG_IGN     ((void (*)(int))1)
#define SIG_HOLD    ((void (*)(int))2)

#define SA_RESTART      (1 << 0)
#define SA_RESETHAND    (1 << 1)
#define SA_SIGINFO      (1 << 2)

typedef void (*sighandler_t)();
typedef void (*sigrestorer_t)(void);

typedef unsigned long sigset_t;

union sigval
{
    int    sival_int;   // integer signal value
    void*  sival_ptr;   // pointer signal value
};

typedef union sigval sigval_t;

#define SI_USER     0
#define SI_KERNEL   0x80
#define SI_TIMER    0x81

#define ILL_ILLOPC 0
#define ILL_ILLOPN 1
#define ILL_ILLADR 2
#define ILL_ILLTRP 3
#define ILL_PRVOPC 4
#define ILL_PRVREG 5
#define ILL_COPROC 6
#define ILL_BADSTK 7

#define FPE_INTDIV 0
#define FPE_INTOVF 1
#define FPE_FLTDIV 2
#define FPE_FLTOVF 3
#define FPE_FLTUND 4
#define FPE_FLTRES 5
#define FPE_FLTINV 6

#define SEGV_MAPERR 0
#define SEGV_ACCERR 1

#define BUS_ADRALN 0
#define BUS_ADRERR 1
#define BUS_OBJERR 2

#define TRAP_BRKPT 0
#define TRAP_TRACE 1

typedef struct
{
    int          si_signo;  // Signal number
    int          si_code;   // Signal code
    int          si_errno;  // If non-zero, an errno value associated with
                            // this signal, as defined in <errno.h>

    union
    {
        // kill
        struct
        {
            pid_t       _si_pid;     // Sending process ID
            uid_t       _si_uid;     // Real user ID of sending process
        } _kill;

        // POSIX.1b timers
        struct
        {
            int         _si_tid;     // Timer ID
            int         _si_overrun; // Overrun count
            sigval_t    _si_sigval;  // Signal value
        } _timer;

        // SIGILL, SIGFPE, SIGSEGV, SIGBUS
        struct
        {
            void*       _si_addr;    // Memory location which caused fault
        } _sigfault;

        // SIGCHLD
        struct
        {
            pid_t       _si_pid;     // Sending process ID
            uid_t       _si_uid;     // Real user ID of sending process
            int         _si_status;  // Exit value or signal
        } _sigchld;
    };
} siginfo_t;

#define si_pid      _kill._si_pid
#define si_uid      _kill._si_uid
#define si_addr     _sigfault._si_addr
#define si_status   _sigchld._si_status
#define si_value    _timer._si_sigval
#define si_int      _timer._si_sigval.sival_int
#define si_ptr      _timer._si_sigval.sival_ptr

struct sigaction
{
    union
    {
        void     (*sa_handler)(int);
        void     (*sa_sigaction)(int, siginfo_t*, void*);
    };
    sigset_t sa_mask;
    int      sa_flags;
    void     (*sa_restorer)(void);
};

#define SIGEV_NONE      0
#define SIGEV_SIGNAL    1
#define SIGEV_THREAD    2

struct sigevent
{
    int          sigev_notify;                   // notification type
    int          sigev_signo;                    // signal number
    union sigval sigev_value;                    // signal value
    void         (*sigev_notify_function)(union sigval); // notification function
    void*        sigev_notify_attributes;        // notification attributes
};

int signal(int signum, sighandler_t handler);
int kill(int pid, int signum);
int sigaction(int signum, const struct sigaction* act, struct sigaction* oldact);
