/*
* HISTORY
* $Log: signal.c,v $
* Revision 1.1 2026/02/15 00:15:30 pedro
* Microkernel version of signal handling.
* All signal operations now send IPC to signal server.
* Signal handlers become IPC ports in the server.
* do_signal now requests signal delivery from server.
* [2026/02/15 pedro]
*/

/*
 * File: kernel/signal.c
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * Signal handling for microkernel architecture.
 *
 * Original Linux 0.11: Direct kernel signal handling with function pointers.
 * Microkernel version: All signal operations are delegated to the signal server.
 * Signal handlers are now IPC ports in the signal server, and signal delivery
 * involves IPC messages from the signal server to the task's signal port.
 *
 * Security: Signal operations require appropriate capabilities:
 * - CAP_SIGNAL_ACTION: Can change signal handlers
 * - CAP_SIGNAL_KILL: Can send signals to other tasks
 * - CAP_SIGNAL_MASK: Can change signal mask
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/head.h>
#include <asm/segment.h>
#include <signal.h>

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_SIGNAL_SGETMASK	0x1C00	/* Get signal mask */
#define MSG_SIGNAL_SSETMASK	0x1C01	/* Set signal mask */
#define MSG_SIGNAL_SIGNAL	0x1C02	/* Signal syscall */
#define MSG_SIGNAL_SIGACTION	0x1C03	/* Sigaction syscall */
#define MSG_SIGNAL_DO_SIGNAL	0x1C04	/* Deliver signal */
#define MSG_SIGNAL_REPLY	0x1C05	/* Reply from signal server */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_signal_mask {
	struct mk_msg_header header;
	int newmask;			/* New mask (for ssetmask) */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_signal_signal {
	struct mk_msg_header header;
	int signum;			/* Signal number */
	long handler;			/* Handler function */
	long restorer;			/* Restorer function */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_signal_sigaction {
	struct mk_msg_header header;
	int signum;			/* Signal number */
	struct sigaction action;	/* New action */
	struct sigaction oldaction;	/* Old action (output) */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_signal_do_signal {
	struct mk_msg_header header;
	long signr;			/* Signal number */
	long eax, ebx, ecx, edx;	/* Registers */
	long fs, es, ds;		/* Segments */
	long eip, cs, eflags;		/* Instruction state */
	unsigned long *esp;		/* Stack pointer */
	long ss;			/* Stack segment */
	unsigned int task_id;		/* Target task */
	capability_t caps;		/* Caller capabilities */
};

struct msg_signal_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		int oldmask;		/* Old signal mask */
		long handler;		/* Old handler */
		struct sigaction oldaction;	/* Old sigaction */
	} data;
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * CAPABILITY FLAGS
 *============================================================================*/

#define CAP_SIGNAL_ACTION	0x0001	/* Can change signal handlers */
#define CAP_SIGNAL_KILL		0x0002	/* Can send signals */
#define CAP_SIGNAL_MASK		0x0004	/* Can change signal mask */

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * signal_request - Send signal request to signal server
 * @msg_id: Message ID
 * @need_reply: Whether to wait for reply
 * @reply_data: Output reply data (if needed)
 * 
 * Returns result from signal server.
 */
static inline int signal_request(unsigned int msg_id, void *msg_data,
                                   int msg_size, int need_reply,
                                   void *reply_data)
{
	struct msg_signal_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	result = mk_msg_send(kernel_state->signal_server, msg_data, msg_size);
	if (result < 0)
		return -1;

	if (!need_reply)
		return 0;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	if (reply_data)
		*(struct msg_signal_reply *)reply_data = reply;

	return reply.result;
}

/*=============================================================================
 * SYSTEM CALL IMPLEMENTATIONS (IPC Stubs)
 *============================================================================*/

/**
 * sys_sgetmask - Get current signal mask
 * 
 * Returns current blocked signal mask.
 */
int sys_sgetmask(void)
{
	struct msg_signal_mask msg;
	struct msg_signal_reply reply;
	int result;

	/* Check capability */
	if (!(current_capability & CAP_SIGNAL_MASK))
		return -1;

	msg.header.msg_id = MSG_SIGNAL_SGETMASK;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = signal_request(MSG_SIGNAL_SGETMASK, &msg, sizeof(msg), 1, &reply);
	if (result < 0)
		return current->blocked;  /* Fallback to local */

	return reply.data.oldmask;
}

/**
 * sys_ssetmask - Set signal mask
 * @newmask: New mask value
 * 
 * Returns old mask value.
 */
int sys_ssetmask(int newmask)
{
	struct msg_signal_mask msg;
	struct msg_signal_reply reply;
	int result;

	if (!(current_capability & CAP_SIGNAL_MASK))
		return -1;

	/* SIGKILL cannot be blocked */
	newmask &= ~(1 << (SIGKILL - 1));

	msg.header.msg_id = MSG_SIGNAL_SSETMASK;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.newmask = newmask;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = signal_request(MSG_SIGNAL_SSETMASK, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		int old = current->blocked;
		current->blocked = newmask;
		return old;
	}

	return reply.data.oldmask;
}

/**
 * sys_signal - Simplified signal handling (original API)
 * @signum: Signal number
 * @handler: Handler function
 * @restorer: Restorer function
 * 
 * Returns old handler.
 */
int sys_signal(int signum, long handler, long restorer)
{
	struct msg_signal_signal msg;
	struct msg_signal_reply reply;
	int result;

	/* Validate signal number */
	if (signum < 1 || signum > 32 || signum == SIGKILL)
		return -1;

	if (!(current_capability & CAP_SIGNAL_ACTION))
		return -1;

	msg.header.msg_id = MSG_SIGNAL_SIGNAL;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.signum = signum;
	msg.handler = handler;
	msg.restorer = restorer;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = signal_request(MSG_SIGNAL_SIGNAL, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local handling */
		struct sigaction tmp;
		long old = (long) current->sigaction[signum-1].sa_handler;
		
		tmp.sa_handler = (void (*)(int)) handler;
		tmp.sa_mask = 0;
		tmp.sa_flags = SA_ONESHOT | SA_NOMASK;
		tmp.sa_restorer = (void (*)(void)) restorer;
		current->sigaction[signum-1] = tmp;
		
		return old;
	}

	return reply.data.handler;
}

/**
 * sys_sigaction - Examine and change signal action
 * @signum: Signal number
 * @action: New action (NULL to query)
 * @oldaction: Old action output (NULL to ignore)
 * 
 * Returns 0 on success, -1 on error.
 */
int sys_sigaction(int signum, const struct sigaction * action,
                   struct sigaction * oldaction)
{
	struct msg_signal_sigaction msg;
	struct msg_signal_reply reply;
	struct sigaction tmp;
	int result;

	/* Validate signal number */
	if (signum < 1 || signum > 32 || signum == SIGKILL)
		return -1;

	if (!(current_capability & CAP_SIGNAL_ACTION))
		return -1;

	/* Copy action from user space if provided */
	if (action) {
		int i;
		char *from = (char *)action;
		char *to = (char *)&tmp;
		
		for (i = 0; i < sizeof(struct sigaction); i++)
			*(to++) = get_fs_byte(from++);
	}

	msg.header.msg_id = MSG_SIGNAL_SIGACTION;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.signum = signum;
	if (action)
		msg.action = tmp;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = signal_request(MSG_SIGNAL_SIGACTION, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local handling */
		tmp = current->sigaction[signum-1];
		
		if (action) {
			current->sigaction[signum-1] = tmp;
			if (current->sigaction[signum-1].sa_flags & SA_NOMASK)
				current->sigaction[signum-1].sa_mask = 0;
			else
				current->sigaction[signum-1].sa_mask |= (1 << (signum-1));
		}
		
		if (oldaction) {
			char *from = (char *)&tmp;
			char *to = (char *)oldaction;
			int i;
			
			verify_area(oldaction, sizeof(struct sigaction));
			for (i = 0; i < sizeof(struct sigaction); i++)
				put_fs_byte(*(from++), to++);
		}
		
		return 0;
	}

	/* Copy oldaction to user space if provided */
	if (oldaction && reply.result == 0) {
		char *from = (char *)&reply.data.oldaction;
		char *to = (char *)oldaction;
		int i;
		
		verify_area(oldaction, sizeof(struct sigaction));
		for (i = 0; i < sizeof(struct sigaction); i++)
			put_fs_byte(*(from++), to++);
	}

	return reply.result;
}

/*=============================================================================
 * SIGNAL DELIVERY
 *============================================================================*/

/**
 * do_signal - Deliver signal to current task
 * @signr: Signal number
 * @eax, ebx, ecx, edx: Register values
 * @fs, es, ds: Segment registers
 * @eip, cs, eflags: Instruction state
 * @esp: Stack pointer
 * @ss: Stack segment
 *
 * This function is called when a signal is to be delivered to the current task.
 * In microkernel mode, it requests the signal server to set up the signal
 * handler context.
 */
void do_signal(long signr, long eax, long ebx, long ecx, long edx,
                long fs, long es, long ds,
                long eip, long cs, long eflags,
                unsigned long * esp, long ss)
{
	struct msg_signal_do_signal msg;
	struct msg_signal_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	/* Check if signal is ignored or default */
	if (signr == SIGCHLD && current->sigaction[signr-1].sa_handler == NULL)
		return;

	/* Ask signal server to handle delivery */
	msg.header.msg_id = MSG_SIGNAL_DO_SIGNAL;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.signr = signr;
	msg.eax = eax;
	msg.ebx = ebx;
	msg.ecx = ecx;
	msg.edx = edx;
	msg.fs = fs;
	msg.es = es;
	msg.ds = ds;
	msg.eip = eip;
	msg.cs = cs;
	msg.eflags = eflags;
	msg.esp = esp;
	msg.ss = ss;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->signal_server, &msg, sizeof(msg));
	if (result < 0) {
		/* Fallback to local delivery */
		do_signal_local(signr, eax, ebx, ecx, edx, fs, es, ds,
		                eip, cs, eflags, esp, ss);
		return;
	}

	/* Wait for server to set up handler context */
	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0) {
		/* Server failed - exit */
		do_exit(1 << (signr - 1));
	}
}

/**
 * do_signal_local - Local signal delivery (fallback)
 * @signr: Signal number
 * @...: Register state
 *
 * Original signal delivery implementation for when signal server is unavailable.
 */
static void do_signal_local(long signr, long eax, long ebx, long ecx, long edx,
                             long fs, long es, long ds,
                             long eip, long cs, long eflags,
                             unsigned long * esp, long ss)
{
	unsigned long sa_handler;
	long old_eip = eip;
	struct sigaction * sa = current->sigaction + signr - 1;
	int longs;
	unsigned long * tmp_esp;

	sa_handler = (unsigned long) sa->sa_handler;
	
	/* Ignore signal */
	if (sa_handler == 1)
		return;
	
	/* Default action */
	if (!sa_handler) {
		if (signr == SIGCHLD)
			return;
		else
			do_exit(1 << (signr - 1));
	}
	
	/* One-shot handler */
	if (sa->sa_flags & SA_ONESHOT)
		sa->sa_handler = NULL;
	
	/* Set up stack for signal handler */
	*(&eip) = sa_handler;
	longs = (sa->sa_flags & SA_NOMASK) ? 7 : 8;
	*(&esp) -= longs;
	verify_area(esp, longs * 4);
	tmp_esp = esp;
	
	put_fs_long((long) sa->sa_restorer, tmp_esp++);
	put_fs_long(signr, tmp_esp++);
	if (!(sa->sa_flags & SA_NOMASK))
		put_fs_long(current->blocked, tmp_esp++);
	put_fs_long(eax, tmp_esp++);
	put_fs_long(ecx, tmp_esp++);
	put_fs_long(edx, tmp_esp++);
	put_fs_long(eflags, tmp_esp++);
	put_fs_long(old_eip, tmp_esp++);
	
	/* Block signals during handler */
	current->blocked |= sa->sa_mask;
}

/*=============================================================================
 * SIGNAL SENDING FUNCTIONS
 *============================================================================*/

/**
 * send_signal - Send signal to task
 * @signr: Signal number
 * @task: Target task
 * 
 * Returns 0 on success, -1 on error.
 */
int send_signal(int signr, struct task_struct * task)
{
	struct msg_signal_do_signal msg;
	
	if (!task)
		return -1;
	
	if (!(current_capability & CAP_SIGNAL_KILL) && 
	    task != current &&
	    !(current_capability & CAP_SYSTEM))
		return -1;

	msg.header.msg_id = MSG_SIGNAL_DO_SIGNAL;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;  /* Fire and forget */
	msg.header.size = sizeof(msg);
	
	msg.signr = signr;
	msg.task_id = task->pid;
	msg.caps = current_capability;

	return mk_msg_send(kernel_state->signal_server, &msg, sizeof(msg));
}

/**
 * kill_proc - Kill process with signal
 * @pid: Process ID
 * @signr: Signal number
 * 
 * Returns 0 on success, -1 on error.
 */
int kill_proc(int pid, int signr)
{
	struct task_struct *task;
	
	for (task = &LAST_TASK; task >= &FIRST_TASK; task--) {
		if (!*task)
			continue;
		if ((*task)->pid == pid)
			return send_signal(signr, *task);
	}
	return -1;  /* ESRCH */
}

/*=============================================================================
 * INITIALIZATION
 *============================================================================*/

/**
 * signal_init - Initialize signal handling
 */
void signal_init(void)
{
	printk("Initializing signal handling (microkernel mode)...\n");
	
	/* Nothing to do locally - signal server handles it */
	
	printk("Signal handling initialized.\n");
}
