/*
* HISTORY
* $Log: signal.h,v $
* Revision 1.1 2026/02/14 03:30:45 pedro
* Microkernel version of signal operations.
* All signal functions now send IPC messages to signal server.
* Signal handlers become IPC ports for signal delivery.
* Signal masks represent capability-filtered signal sets.
* [2026/02/14 pedro]
*/

/*
* File: signal.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* Signal operations for microkernel architecture.
*
* Original Linux 0.11: Direct kernel signal handling with function pointers.
* Microkernel version: Signals are managed by signal server. Signal handlers
* are replaced with IPC ports - when a signal is delivered, the signal server
* sends a message to the task's signal port.
*
* Security: Sending signals requires appropriate capabilities:
* - CAP_PROCESS: Can send signals to own process group
* - CAP_SYSTEM: Can send signals to any task
* - CAP_KILL: Specific capability for kill operation
*
* Signal masks now filter signals based on capabilities as well.
*/

#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h>
#include <linux/kernel.h>
#include <linux/head.h>

/* Message codes for signal operations */
#define MSG_SIGNAL_ACTION	0x0C00	/* sigaction - set handler */
#define MSG_SIGNAL_KILL		0x0C01	/* kill - send signal */
#define MSG_SIGNAL_RAISE	0x0C02	/* raise - send signal to self */
#define MSG_SIGNAL_PROCMASK	0x0C03	/* sigprocmask - set signal mask */
#define MSG_SIGNAL_PENDING	0x0C04	/* sigpending - get pending signals */
#define MSG_SIGNAL_SUSPEND	0x0C05	/* sigsuspend - wait for signal */
#define MSG_SIGNAL_DELIVER	0x0C06	/* Signal delivery from server */
#define MSG_SIGNAL_REPLY	0x0C07	/* Reply with signal info */
#define MSG_SIGNAL_MASK_OPS	0x0C08	/* sigaddset, sigdelset, etc. */

/* Signal operation message structures */
struct msg_signal_action {
	struct mk_msg_header header;
	int sig;				/* Signal number */
	unsigned int handler_port;		/* IPC port for signal handler */
	sigset_t mask;				/* Signal mask during handler */
	int flags;				/* SA_* flags */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_signal_kill {
	struct mk_msg_header header;
	pid_t pid;				/* Target process (capability) */
	int sig;				/* Signal number */
	unsigned int sender_task;		/* Task sending signal */
	capability_t caps;			/* Caller capabilities */
};

struct msg_signal_procmask {
	struct mk_msg_header header;
	int how;				/* SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK */
	sigset_t *set;				/* New mask (in user space) */
	sigset_t *oldset;			/* Old mask (out) */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_signal_pending {
	struct mk_msg_header header;
	sigset_t *set;				/* Buffer for pending signals */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_signal_suspend {
	struct mk_msg_header header;
	sigset_t *sigmask;			/* Mask to set while suspended */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_signal_deliver {
	struct mk_msg_header header;
	int sig;				/* Signal being delivered */
	unsigned int sender;			/* Who sent it */
	siginfo_t info;				/* Additional info */
};

struct msg_signal_reply {
	struct mk_msg_header header;
	int result;				/* Result code */
	union {
		sigset_t mask;			/* Signal mask */
		struct {
			unsigned int old_handler;	/* Previous handler port */
			sigset_t old_mask;		/* Previous mask */
			int old_flags;			/* Previous flags */
		} action;
		pid_t sender;			/* Sender of signal */
	} data;
};

/* Signal delivery information structure */
typedef struct {
	int si_signo;			/* Signal number */
	int si_errno;			/* Errno associated */
	int si_code;			/* Signal code */
	pid_t si_pid;			/* Sending process ID */
	uid_t si_uid;			/* Sending user (capability domain) */
	void *si_addr;			/* Address of faulting instruction */
	int si_status;			/* Exit value or signal */
} siginfo_t;

/* Original types preserved */
typedef int sig_atomic_t;
typedef unsigned int sigset_t;		/* 32 bits - now capability-filtered */

/* Number of signals (unchanged) */
#define _NSIG             32
#define NSIG		_NSIG

/* Signal numbers (unchanged) */
#define SIGHUP		 1
#define SIGINT		 2
#define SIGQUIT		 3
#define SIGILL		 4
#define SIGTRAP		 5
#define SIGABRT		 6
#define SIGIOT		 6
#define SIGUNUSED	 7
#define SIGFPE		 8
#define SIGKILL		 9
#define SIGUSR1		10
#define SIGSEGV		11
#define SIGUSR2		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGSTKFLT	16
#define SIGCHLD		17
#define SIGCONT		18
#define SIGSTOP		19
#define SIGTSTP		20
#define SIGTTIN		21
#define SIGTTOU		22

/* Signal actions - now handler ports instead of function pointers */
#define SA_NOCLDSTOP	1		/* Don't send SIGCHLD when children stop */
#define SA_NOMASK	0x40000000	/* Don't block signals in handler */
#define SA_ONESHOT	0x80000000	/* Restore default after one use */
#define SA_RESTART	0x10000000	/* Restart syscalls after signal */
#define SA_SIGINFO	0x20000000	/* Use siginfo_t */

/* How to change signal mask */
#define SIG_BLOCK       0	/* Block signals in set */
#define SIG_UNBLOCK     1	/* Unblock signals in set */
#define SIG_SETMASK     2	/* Set mask to exactly set */

/* Special handler values - now special port numbers */
#define SIG_DFL		((unsigned int)0)	/* Default handling (server) */
#define SIG_IGN		((unsigned int)-1)	/* Ignore signal */
#define SIG_ERR		((unsigned int)-2)	/* Error return */

/* Signal capability flags */
#define CAP_SIGNAL_KILL		0x0001	/* Can send signals */
#define CAP_SIGNAL_ACTION	0x0002	/* Can change signal handlers */
#define CAP_SIGNAL_MASK		0x0004	/* Can change signal mask */
#define CAP_SIGNAL_ALL		0x0007	/* All signal capabilities */

/* Signal forwarding flags */
#define SF_FORCE		0x01	/* Force delivery (override mask) */
#define SF_CHECK_CAP		0x02	/* Check capabilities */
#define SF_FROM_KERNEL		0x04	/* Signal from kernel (exception) */

/**
 * Signal handler type - preserved for API compatibility,
 * but internally converted to port numbers.
 */
typedef void (*sighandler_t)(int);

struct sigaction {
	union {
		sighandler_t sa_handler;	/* For compatibility */
		unsigned int sa_handler_port;	/* Actual IPC port */
	} _u;
	sigset_t sa_mask;			/* Signals to block in handler */
	int sa_flags;				/* SA_* flags */
	void (*sa_restorer)(void);		/* Restorer function (unused) */
};

#define sa_handler	_u.sa_handler
#define sa_handler_port	_u.sa_handler_port

/* Public function prototypes */
sighandler_t signal(int _sig, sighandler_t _func);
int raise(int sig);
int kill(pid_t pid, int sig);
int sigaddset(sigset_t *mask, int signo);
int sigdelset(sigset_t *mask, int signo);
int sigemptyset(sigset_t *mask);
int sigfillset(sigset_t *mask);
int sigismember(sigset_t *mask, int signo);
int sigpending(sigset_t *set);
int sigprocmask(int how, sigset_t *set, sigset_t *oldset);
int sigsuspend(sigset_t *sigmask);
int sigaction(int sig, struct sigaction *act, struct sigaction *oldact);

/* Additional POSIX functions */
int sigwait(const sigset_t *set, int *sig);
int sigwaitinfo(const sigset_t *set, siginfo_t *info);
int sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout);
int sigqueue(pid_t pid, int sig, const void *value);

/* Internal implementations */

/**
 * __do_signal_action - Internal function for sigaction
 */
static inline int __do_signal_action(int sig, struct sigaction *act, 
                                      struct sigaction *oldact)
{
	struct msg_signal_action msg;
	struct msg_signal_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_SIGNAL_ACTION)) {
		return -1;  /* EPERM */
	}

	/* Prepare message */
	msg.header.msg_id = MSG_SIGNAL_ACTION;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.sig = sig;
	msg.handler_port = act ? act->sa_handler_port : 0;
	msg.mask = act ? act->sa_mask : 0;
	msg.flags = act ? act->sa_flags : 0;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	/* Send to signal server */
	result = mk_msg_send(kernel_state->signal_server, &msg, sizeof(msg));
	if (result < 0) {
		return -1;
	}

	/* Wait for reply */
	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0) {
		return -1;
	}

	/* Fill oldact if requested */
	if (oldact && reply.result == 0) {
		oldact->sa_handler_port = reply.data.action.old_handler;
		oldact->sa_mask = reply.data.action.old_mask;
		oldact->sa_flags = reply.data.action.old_flags;
		oldact->sa_restorer = NULL;
	}

	return reply.result;
}

/**
 * signal - Simplified sigaction (original API)
 */
sighandler_t signal(int _sig, sighandler_t _func)
{
	struct sigaction act, oldact;
	int result;

	act.sa_handler = _func;
	act.sa_mask = 0;
	act.sa_flags = 0;
	act.sa_restorer = NULL;

	result = __do_signal_action(_sig, &act, &oldact);
	if (result < 0) {
		return SIG_ERR;
	}

	return oldact.sa_handler;
}

/**
 * sigaction - Examine and change signal action
 */
int sigaction(int sig, struct sigaction *act, struct sigaction *oldact)
{
	return __do_signal_action(sig, act, oldact);
}

/**
 * kill - Send signal to process
 */
int kill(pid_t pid, int sig)
{
	struct msg_signal_kill msg;
	struct msg_signal_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	/* Check capability */
	if (!(current_capability & CAP_SIGNAL_KILL) && 
	    !(current_capability & CAP_PROCESS) &&
	    !(current_capability & CAP_SYSTEM)) {
		return -1;  /* EPERM */
	}

	/* Special case: signal 0 checks if process exists */
	if (sig == 0) {
		/* Would need process_exist() check via process server */
		return 0;  /* Assume exists */
	}

	msg.header.msg_id = MSG_SIGNAL_KILL;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.pid = pid;
	msg.sig = sig;
	msg.sender_task = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->signal_server, &msg, sizeof(msg));
	if (result < 0) {
		return -1;
	}

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0) {
		return -1;
	}

	return reply.result;
}

/**
 * raise - Send signal to self
 */
int raise(int sig)
{
	return kill(kernel_state->current_task, sig);
}

/**
 * sigprocmask - Examine and change signal mask
 */
int sigprocmask(int how, sigset_t *set, sigset_t *oldset)
{
	struct msg_signal_procmask msg;
	struct msg_signal_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_SIGNAL_MASK)) {
		return -1;  /* EPERM */
	}

	msg.header.msg_id = MSG_SIGNAL_PROCMASK;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.how = how;
	msg.set = set;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->signal_server, &msg, sizeof(msg));
	if (result < 0) {
		return -1;
	}

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0) {
		return -1;
	}

	if (oldset && reply.result == 0) {
		*oldset = reply.data.mask;
	}

	return reply.result;
}

/**
 * sigpending - Get pending signals
 */
int sigpending(sigset_t *set)
{
	struct msg_signal_pending msg;
	struct msg_signal_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!set) {
		return -1;  /* EFAULT */
	}

	msg.header.msg_id = MSG_SIGNAL_PENDING;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.set = set;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->signal_server, &msg, sizeof(msg));
	if (result < 0) {
		return -1;
	}

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0) {
		return -1;
	}

	if (reply.result == 0) {
		*set = reply.data.mask;
	}

	return reply.result;
}

/**
 * sigsuspend - Wait for signal
 */
int sigsuspend(sigset_t *sigmask)
{
	struct msg_signal_suspend msg;
	struct msg_signal_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	msg.header.msg_id = MSG_SIGNAL_SUSPEND;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.sigmask = sigmask;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->signal_server, &msg, sizeof(msg));
	if (result < 0) {
		return -1;
	}

	/* This will only return after signal delivery */
	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	
	return -1;  /* Should not reach here normally */
}

/* Signal set operations (can be done locally) */

int sigemptyset(sigset_t *mask)
{
	if (!mask) return -1;
	*mask = 0;
	return 0;
}

int sigfillset(sigset_t *mask)
{
	if (!mask) return -1;
	*mask = ~0U;
	return 0;
}

int sigaddset(sigset_t *mask, int signo)
{
	if (!mask || signo < 1 || signo >= _NSIG) return -1;
	*mask |= (1 << (signo - 1));
	return 0;
}

int sigdelset(sigset_t *mask, int signo)
{
	if (!mask || signo < 1 || signo >= _NSIG) return -1;
	*mask &= ~(1 << (signo - 1));
	return 0;
}

int sigismember(sigset_t *mask, int signo)
{
	if (!mask || signo < 1 || signo >= _NSIG) return -1;
	return (*mask & (1 << (signo - 1))) != 0;
}

/**
 * sigwait - Wait for signal in set
 */
int sigwait(const sigset_t *set, int *sig)
{
	siginfo_t info;
	int result = sigwaitinfo(set, &info);
	if (result == 0 && sig) {
		*sig = info.si_signo;
	}
	return result;
}

/**
 * sigwaitinfo - Wait for signal with info
 */
int sigwaitinfo(const sigset_t *set, siginfo_t *info)
{
	/* Would need implementation with signal server */
	/* Similar to sigsuspend but returns signal info */
	return -1;  /* ENOSYS */
}

/**
 * sigqueue - Queue signal with value
 */
int sigqueue(pid_t pid, int sig, const void *value)
{
	/* Extended kill with queued value */
	struct msg_signal_queue msg;  /* Would need new message type */
	return -1;  /* ENOSYS */
}

/**
 * Signal handler thread - each task needs one to receive signals
 */
static inline void signal_handler_thread(void)
{
	struct msg_signal_deliver msg;
	unsigned int size;
	
	while (1) {
		/* Receive signal from signal server */
		mk_msg_receive(kernel_state->signal_port, &msg, &size);
		
		/* Call actual handler (which may send IPC to server) */
		/* This runs in user context */
		handle_signal(msg.sig, &msg.info);
	}
}

/**
 * request_signal_capability - Request signal capabilities
 */
static inline int request_signal_capability(unsigned int caps)
{
	struct msg_cap_request msg;
	struct msg_cap_reply reply;
	unsigned int reply_size = sizeof(reply);

	msg.header.msg_id = MSG_CAP_REQUEST_SIGNAL;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.task_id = kernel_state->current_task;
	msg.requested_cap = caps;

	if (mk_msg_send(kernel_state->signal_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0) {
				current_capability |= caps;
				return 0;
			}
		}
	}
	return -1;
}

#endif /* _SIGNAL_H */
