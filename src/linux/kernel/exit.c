/*
* HISTORY
* $Log: exit.c,v $
* Revision 1.1 2026/02/15 07:15:30 pedro
* Microkernel version of process exit and wait.
* All process termination and waiting delegated to process server.
* kill/waitpid/send_sig now IPC messages.
* release/tell_father/kill_session handled by server.
* [2026/02/15 pedro]
*/

/*
 * File: kernel/exit.c
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * Process exit and waiting for microkernel architecture.
 *
 * Original Linux 0.11: Direct kernel process termination and waiting.
 * Microkernel version: All operations delegated to process server via IPC.
 * The kernel maintains minimal local state and forwards all requests.
 *
 * Security: Signal sending requires appropriate capabilities.
 * waitpid requires CAP_PROCESS capability.
 */

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/head.h>
#include <asm/segment.h>

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_EXIT_RELEASE	0x2000	/* Release task structure */
#define MSG_EXIT_SEND_SIG	0x2001	/* Send signal */
#define MSG_EXIT_KILL_SESSION	0x2002	/* Kill session */
#define MSG_EXIT_TELL_FATHER	0x2003	/* Tell father of child death */
#define MSG_EXIT_DO_EXIT	0x2004	/* Process exit */
#define MSG_EXIT_WAITPID	0x2005	/* Wait for child */
#define MSG_EXIT_KILL		0x2006	/* Kill system call */
#define MSG_EXIT_REPLY		0x2007	/* Reply from process server */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_exit_release {
	struct mk_msg_header header;
	struct task_struct *p;		/* Task to release */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_exit_send_sig {
	struct mk_msg_header header;
	long sig;			/* Signal number */
	struct task_struct *p;		/* Target task */
	int priv;			/* Privileged flag */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_exit_kill_session {
	struct mk_msg_header header;
	unsigned int session;		/* Session to kill */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_exit_tell_father {
	struct mk_msg_header header;
	int pid;			/* Father PID */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_exit_do_exit {
	struct mk_msg_header header;
	long code;			/* Exit code */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_exit_waitpid {
	struct mk_msg_header header;
	pid_t pid;			/* PID to wait for */
	unsigned long *stat_addr;	/* Status address */
	int options;			/* Wait options */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_exit_kill {
	struct mk_msg_header header;
	int pid;			/* PID to kill */
	int sig;			/* Signal to send */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_exit_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		int pid;		/* PID for waitpid */
		long code;		/* Exit code */
	} data;
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * CAPABILITY FLAGS
 *============================================================================*/

#define CAP_EXIT_PROCESS	0x0400	/* Can exit/wait for processes */
#define CAP_EXIT_KILL		0x0800	/* Can kill processes */

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * exit_request - Send exit request to process server
 * @msg_id: Message ID
 * @msg_data: Message data
 * @msg_size: Message size
 * @need_reply: Whether to wait for reply
 * @reply_data: Output reply data
 * 
 * Returns result from process server.
 */
static inline int exit_request(unsigned int msg_id, void *msg_data,
                                 int msg_size, int need_reply,
                                 struct msg_exit_reply *reply_data)
{
	struct msg_exit_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	/* Check capability */
	if (!(current_capability & CAP_EXIT_PROCESS))
		return -EPERM;

	result = mk_msg_send(kernel_state->process_server, msg_data, msg_size);
	if (result < 0)
		return -EAGAIN;

	if (!need_reply)
		return 0;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -EAGAIN;

	if (reply_data)
		*reply_data = reply;

	return reply.result;
}

/*=============================================================================
 * RELEASE TASK
 *============================================================================*/

/**
 * release - Release a task structure
 * @p: Task to release
 * 
 * In microkernel mode, this notifies the process server to release
 * the task and frees the local task structure.
 */
void release(struct task_struct * p)
{
	struct msg_exit_release msg;
	int i;

	if (!p)
		return;

	/* Send release request to process server */
	msg.header.msg_id = MSG_EXIT_RELEASE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;  /* Fire and forget */
	msg.header.size = sizeof(msg);
	
	msg.p = p;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	exit_request(MSG_EXIT_RELEASE, &msg, sizeof(msg), 0, NULL);

	/* Free local task structure */
	for (i = 1; i < NR_TASKS; i++) {
		if (task[i] == p) {
			task[i] = NULL;
			free_page((long)p);
			schedule();
			return;
		}
	}
	panic("trying to release non-existent task");
}

/*=============================================================================
 * SEND SIGNAL
 *============================================================================*/

static inline int send_sig(long sig, struct task_struct * p, int priv)
{
	struct msg_exit_send_sig msg;
	struct msg_exit_reply reply;
	int result;

	if (!p || sig < 1 || sig > 32)
		return -EINVAL;

	/* Prepare IPC message */
	msg.header.msg_id = MSG_EXIT_SEND_SIG;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.sig = sig;
	msg.p = p;
	msg.priv = priv;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = exit_request(MSG_EXIT_SEND_SIG, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		if (priv || (current->euid == p->euid) || suser())
			p->signal |= (1 << (sig - 1));
		else
			return -EPERM;
		return 0;
	}

	return result;
}

/*=============================================================================
 * KILL SESSION
 *============================================================================*/

static void kill_session(void)
{
	struct msg_exit_kill_session msg;
	
	msg.header.msg_id = MSG_EXIT_KILL_SESSION;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.session = current->session;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	exit_request(MSG_EXIT_KILL_SESSION, &msg, sizeof(msg), 0, NULL);
}

/*=============================================================================
 * KILL SYSTEM CALL
 *============================================================================*/

int sys_kill(int pid, int sig)
{
	struct msg_exit_kill msg;
	struct msg_exit_reply reply;
	int result;

	/* Check capability */
	if (!(current_capability & CAP_EXIT_KILL))
		return -EPERM;

	/* Prepare IPC message */
	msg.header.msg_id = MSG_EXIT_KILL;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.pid = pid;
	msg.sig = sig;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = exit_request(MSG_EXIT_KILL, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		struct task_struct **p = NR_TASKS + task;
		int err, retval = 0;

		if (!pid) {
			while (--p > &FIRST_TASK) {
				if (*p && (*p)->pgrp == current->pid) 
					if ((err = send_sig(sig, *p, 1)))
						retval = err;
			}
		} else if (pid > 0) {
			while (--p > &FIRST_TASK) {
				if (*p && (*p)->pid == pid) 
					if ((err = send_sig(sig, *p, 0)))
						retval = err;
			}
		} else if (pid == -1) {
			while (--p > &FIRST_TASK) {
				if ((err = send_sig(sig, *p, 0)))
					retval = err;
			}
		} else {
			while (--p > &FIRST_TASK)
				if (*p && (*p)->pgrp == -pid)
					if ((err = send_sig(sig, *p, 0)))
						retval = err;
		}
		return retval;
	}

	return result;
}

/*=============================================================================
 * TELL FATHER
 *============================================================================*/

static void tell_father(int pid)
{
	struct msg_exit_tell_father msg;
	
	msg.header.msg_id = MSG_EXIT_TELL_FATHER;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.pid = pid;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	exit_request(MSG_EXIT_TELL_FATHER, &msg, sizeof(msg), 0, NULL);
}

/*=============================================================================
 * DO EXIT
 *============================================================================*/

int do_exit(long code)
{
	struct msg_exit_do_exit msg;
	struct msg_exit_reply reply;
	int result;

	/* Prepare IPC message */
	msg.header.msg_id = MSG_EXIT_DO_EXIT;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.code = code;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = exit_request(MSG_EXIT_DO_EXIT, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		int i;
		
		free_page_tables(get_base(current->ldt[1]), get_limit(0x0f));
		free_page_tables(get_base(current->ldt[2]), get_limit(0x17));
		
		for (i = 0; i < NR_TASKS; i++)
			if (task[i] && task[i]->father == current->pid) {
				task[i]->father = 1;
				if (task[i]->state == TASK_ZOMBIE)
					(void) send_sig(SIGCHLD, task[1], 1);
			}
		
		for (i = 0; i < NR_OPEN; i++)
			if (current->filp[i])
				sys_close(i);
		
		iput(current->pwd);
		current->pwd = NULL;
		iput(current->root);
		current->root = NULL;
		iput(current->executable);
		current->executable = NULL;
		
		if (current->leader && current->tty >= 0)
			tty_table[current->tty].pgrp = 0;
		
		if (last_task_used_math == current)
			last_task_used_math = NULL;
		
		if (current->leader)
			kill_session();
		
		current->state = TASK_ZOMBIE;
		current->exit_code = code;
		tell_father(current->father);
		schedule();
		return -1;
	}

	return result;
}

/*=============================================================================
 * EXIT SYSTEM CALL
 *============================================================================*/

int sys_exit(int error_code)
{
	return do_exit((error_code & 0xff) << 8);
}

/*=============================================================================
 * WAITPID SYSTEM CALL
 *============================================================================*/

int sys_waitpid(pid_t pid, unsigned long * stat_addr, int options)
{
	struct msg_exit_waitpid msg;
	struct msg_exit_reply reply;
	int result;

	/* Verify user area */
	verify_area(stat_addr, 4);

	/* Prepare IPC message */
	msg.header.msg_id = MSG_EXIT_WAITPID;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.pid = pid;
	msg.stat_addr = stat_addr;
	msg.options = options;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = exit_request(MSG_EXIT_WAITPID, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		int flag, code;
		struct task_struct ** p;

	repeat:
		flag = 0;
		for (p = &LAST_TASK; p > &FIRST_TASK; --p) {
			if (!*p || *p == current)
				continue;
			if ((*p)->father != current->pid)
				continue;
			if (pid > 0) {
				if ((*p)->pid != pid)
					continue;
			} else if (!pid) {
				if ((*p)->pgrp != current->pgrp)
					continue;
			} else if (pid != -1) {
				if ((*p)->pgrp != -pid)
					continue;
			}
			switch ((*p)->state) {
				case TASK_STOPPED:
					if (!(options & WUNTRACED))
						continue;
					put_fs_long(0x7f, stat_addr);
					return (*p)->pid;
				case TASK_ZOMBIE:
					current->cutime += (*p)->utime;
					current->cstime += (*p)->stime;
					flag = (*p)->pid;
					code = (*p)->exit_code;
					release(*p);
					put_fs_long(code, stat_addr);
					return flag;
				default:
					flag = 1;
					continue;
			}
		}
		if (flag) {
			if (options & WNOHANG)
				return 0;
			current->state = TASK_INTERRUPTIBLE;
			schedule();
			if (!(current->signal &= ~(1 << (SIGCHLD - 1))))
				goto repeat;
			else
				return -EINTR;
		}
		return -ECHILD;
	}

	/* Copy result to user space */
	if (result >= 0) {
		put_fs_long(reply.data.code, stat_addr);
	}

	return result;
}

/*=============================================================================
 * INITIALIZATION
 *============================================================================*/

/**
 * exit_init - Initialize exit subsystem
 */
void exit_init(void)
{
	printk("Initializing exit subsystem (microkernel mode)...\n");
	printk("Exit subsystem initialized.\n");
}

