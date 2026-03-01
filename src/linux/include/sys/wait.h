/*
* HISTORY
* $Log: wait.h,v $
* Revision 1.1 2026/02/14 00:15:30 pedro
* Microkernel version of wait operations.
* wait/waitpid now send IPC messages to process server.
* Status codes now represent capability state changes.
* WNOHANG and WUNTRACED flags preserved for compatibility.
* [2026/02/14 pedro]
*/

/*
* File: sys/wait.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* Wait operations for microkernel architecture.
*
* Original Linux 0.11: Direct kernel calls to wait for process state changes.
* Microkernel version: wait/waitpid send IPC messages to the process server,
* which manages task states and capabilities.
*
* Status codes now reflect capability transitions:
* - Exited: Task released all capabilities
* - Signaled: Task terminated by signal (capability revoked)
* - Stopped: Task suspended (capabilities preserved)
*/

#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <sys/types.h>
#include <linux/kernel.h>  /* For kernel_state, mk_msg_send/receive */
#include <linux/head.h>    /* For message structures */

/* Message codes for wait operations */
#define MSG_WAIT		0x0900	/* Wait for any child */
#define MSG_WAITPID		0x0901	/* Wait for specific child */
#define MSG_WAIT_REPLY		0x0902	/* Reply with status */

/* Wait operation message structures */
struct msg_wait {
	struct mk_msg_header header;
	pid_t pid;			/* PID to wait for (0 for any) */
	int options;			/* WNOHANG, WUNTRACED */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_wait_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	pid_t pid;			/* PID that changed state */
	int status;			/* Status information */
	capability_t remaining_caps;	/* Task's remaining capabilities */
};

/* Original macros preserved exactly for compatibility */
#define _LOW(v)		( (v) & 0377)
#define _HIGH(v)	( ((v) >> 8) & 0377)

/* options for waitpid, WUNTRACED not supported */
#define WNOHANG		1
#define WUNTRACED	2

/* Status inspection macros - same interface, but status now includes */
/* capability information in the high bits */
#define WIFEXITED(s)	(!((s)&0xFF))
#define WIFSTOPPED(s)	(((s)&0xFF)==0x7F)
#define WEXITSTATUS(s)	(((s)>>8)&0xFF)
#define WTERMSIG(s)	((s)&0x7F)
#define WSTOPSIG(s)	(((s)>>8)&0xFF)
#define WIFSIGNALED(s)	(((unsigned int)(s)-1 & 0xFFFF) < 0xFF)

/* New macros for capability information (extensions) */
#define WCAPRELEASED(s)	(((s) >> 16) & 0xFF)  /* Capabilities released */
#define WCAPINHERITED(s)	(((s) >> 24) & 0xFF) /* Capabilities inherited */

/**
 * wait - Wait for any child process to change state
 * @stat_loc: Pointer to store status information (can be NULL)
 *
 * Suspends the calling task until any of its child tasks changes state
 * (exits, stops, or is terminated by a signal).
 *
 * In microkernel mode, this sends a message to the process server,
 * which monitors all task state changes and capabilities.
 *
 * Returns the PID of the child that changed state, or -1 on error.
 */
pid_t wait(int *stat_loc);

/**
 * waitpid - Wait for a specific child process to change state
 * @pid: PID of child to wait for (or special values)
 * @stat_loc: Pointer to store status information
 * @options: WNOHANG (don't block) or WUNTRACED (report stopped children)
 *
 * Special pid values:
 * - pid < -1: Wait for any child in process group |pid|
 * - pid == -1: Wait for any child
 * - pid == 0: Wait for any child in same process group as caller
 * - pid > 0: Wait for child with specific PID
 *
 * Returns the PID of the child, 0 if WNOHANG and no child available,
 * or -1 on error.
 */
pid_t waitpid(pid_t pid, int *stat_loc, int options);

/* Internal implementation as inline functions */

/**
 * __do_wait - Internal function to perform wait operation
 * @pid: PID to wait for
 * @stat_loc: Status location
 * @options: Wait options
 *
 * Sends IPC message to process server and waits for reply.
 */
static inline pid_t __do_wait(pid_t pid, int *stat_loc, int options)
{
	struct msg_wait msg;
	struct msg_wait_reply reply;
	unsigned int reply_size = sizeof(reply);
	pid_t result_pid;
	int result;
	
	/* Check if we have capability to wait for processes */
	if (!(current_capability & CAP_PROCESS)) {
		/* Try to request process capability */
		if (request_process_capability() < 0)
			return -1;
	}
	
	/* Prepare wait message */
	msg.header.msg_id = (pid == -1 || pid == 0) ? MSG_WAIT : MSG_WAITPID;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.pid = pid;
	msg.options = options;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	
	/* Send to process server */
	result = mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;
	
	/* Wait for reply (unless WNOHANG and no child available) */
	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;
	
	/* Process reply */
	result_pid = reply.pid;
	
	/* Store status if requested */
	if (stat_loc && reply.result == 0) {
		*stat_loc = reply.status;
	}
	
	/* Update capabilities if task inherited any */
	if (reply.remaining_caps)
		current_capability |= reply.remaining_caps;
	
	return (reply.result < 0) ? reply.result : result_pid;
}

/* Public function implementations */
pid_t wait(int *stat_loc)
{
	return __do_wait(-1, stat_loc, 0);
}

pid_t waitpid(pid_t pid, int *stat_loc, int options)
{
	/* Validate options */
	if (options & ~(WNOHANG | WUNTRACED)) {
		return -1;  /* Invalid options */
	}
	
	return __do_wait(pid, stat_loc, options);
}

/**
 * wait3 - BSD compatibility (wait with rusage)
 * @stat_loc: Status location
 * @options: Wait options
 * @rusage: Resource usage (not supported in microkernel)
 *
 * Simplified version - rusage ignored, forwards to waitpid(-1)
 */
static inline pid_t wait3(int *stat_loc, int options, void *rusage)
{
	/* rusage not supported in microkernel (would need resource server) */
	return waitpid(-1, stat_loc, options);
}

/**
 * wait4 - BSD compatibility (wait for specific pid with rusage)
 * @pid: PID to wait for
 * @stat_loc: Status location
 * @options: Wait options
 * @rusage: Resource usage (not supported)
 */
static inline pid_t wait4(pid_t pid, int *stat_loc, int options, void *rusage)
{
	return waitpid(pid, stat_loc, options);
}

/**
 * request_process_capability - Request process management capability
 *
 * Tasks without CAP_PROCESS can request temporary capability
 * to wait for children.
 */
static inline int request_process_capability(void)
{
	struct msg_cap_request msg;
	struct msg_cap_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	msg.header.msg_id = MSG_CAP_REQUEST_PROCESS;  /* Would need definition */
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.task_id = kernel_state->current_task;
	msg.requested_cap = CAP_PROCESS;
	
	if (mk_msg_send(kernel_state->process_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0) {
				current_capability |= CAP_PROCESS;
				return 0;
			}
		}
	}
	return -1;
}

/**
 * Status code construction helpers (for process server)
 */
static inline int make_exit_status(int exit_code, int caps_released)
{
	return (exit_code << 8) | (caps_released << 16) | 0x00;
}

static inline int make_signal_status(int signal, int caps_released)
{
	return signal | (caps_released << 16) | 0x80;
}

static inline int make_stop_status(int signal, int caps_held)
{
	return 0x7F | (signal << 8) | (caps_held << 16);
}

/**
 * Capability inheritance macros
 */
#define CAPS_INHERIT_ALL	0xFF	/* Inherit all capabilities */
#define CAPS_INHERIT_NONE	0x00	/* Inherit no capabilities */
#define CAPS_INHERIT_DEFAULT	0x0F	/* Inherit basic capabilities */

#endif /* _SYS_WAIT_H */
