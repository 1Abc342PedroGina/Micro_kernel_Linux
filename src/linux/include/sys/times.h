/*
* HISTORY
* $Log: times.h,v $
* Revision 1.1 2026/02/14 02:45:30 pedro
* Microkernel version of process times.
* times() now sends IPC message to process server.
* tms structure preserved for compatibility.
* Added capability checking for process statistics.
* [2026/02/14 pedro]
*/

/*
* File: sys/times.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* Process times for microkernel architecture.
*
* Original Linux 0.11: Direct kernel access to process timing data.
* Microkernel version: times() sends IPC message to process server,
* which maintains timing information for all tasks. The process server
* tracks CPU time consumed by each task and its children.
*
* Time units: clock ticks (usually 100Hz). The time server maintains
* the system clock and provides tick counts to the process server.
*
* Security: Basic process timing is available to all tasks for their
* own processes. Access to other tasks' times requires CAP_PROCESS.
*/

#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H

#include <sys/types.h>
#include <linux/kernel.h>  /* For kernel_state, mk_msg_send/receive */
#include <linux/head.h>    /* For message structures */

/* Message codes for times operation */
#define MSG_TIMES		0x0B00	/* Get process times */
#define MSG_TIMES_REPLY		0x0B01	/* Reply with tms data */
#define MSG_TIMES_CHILD		0x0B02	/* Get child process times */
#define MSG_TIMES_ACCUM		0x0B03	/* Accumulate child times (for wait) */

/* Times operation message structures */
struct msg_times {
	struct mk_msg_header header;
	pid_t pid;			/* PID to get times for (0 = self) */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
	unsigned int flags;		/* Request flags */
};

struct msg_times_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	time_t tms_utime;		/* User CPU time */
	time_t tms_stime;		/* System CPU time */
	time_t tms_cutime;		/* User time of children */
	time_t tms_cstime;		/* System time of children */
	time_t elapsed;			/* Elapsed real time (optional) */
	unsigned int ticks_per_sec;	/* Clock ticks per second */
};

/*
 * tms structure -完全相同 ao original para compatibilidade
 * 
 * Original Linux 0.11: time_t values in clock ticks
 * Microkernel version: Same structure, data comes from process server
 */
struct tms {
	time_t tms_utime;	/* User CPU time used */
	time_t tms_stime;	/* System CPU time used */
	time_t tms_cutime;	/* User CPU time of terminated children */
	time_t tms_cstime;	/* System CPU time of terminated children */
};

/* Flags for times request */
#define TIMES_SELF		0x00	/* Get times for current task */
#define TIMES_PID		0x01	/* Get times for specific PID */
#define TIMES_CHILDREN		0x02	/* Include children times */
#define TIMES_ELAPSED		0x04	/* Include elapsed real time */
#define TIMES_ACCUM		0x08	/* Accumulate and clear child times */

/**
 * times - Get process times
 * @tp: Pointer to tms structure to fill (can be NULL)
 *
 * Obtains CPU time accounting information for the calling process
 * and its terminated children from the process server.
 *
 * The process server maintains:
 * - User CPU time (tms_utime): Time spent in user mode
 * - System CPU time (tms_stime): Time spent in kernel mode
 * - Child user time (tms_cutime): User time of waited-for children
 * - Child system time (tms_cstime): System time of waited-for children
 *
 * Returns the elapsed real time since an arbitrary point in the past
 * (usually system boot) in clock ticks, or -1 on error.
 */
extern time_t times(struct tms *tp);

/* Internal implementation as inline function */
static inline time_t __times(struct tms *tp, pid_t pid, unsigned int flags)
{
	struct msg_times msg;
	struct msg_times_reply reply;
	unsigned int reply_size = sizeof(reply);
	time_t elapsed;
	int result;
	
	/* Check capability for accessing other processes */
	if (pid != 0 && pid != kernel_state->current_task) {
		if (!(current_capability & CAP_PROCESS)) {
			/* Try to request process capability */
			if (request_process_capability() < 0)
				return -1;
		}
	}
	
	/* Prepare times request message */
	msg.header.msg_id = MSG_TIMES;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.pid = (pid == 0) ? kernel_state->current_task : pid;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	msg.flags = flags | (tp ? 0 : TIMES_ELAPSED);  /* Always get elapsed */
	
	/* Send to process server */
	result = mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
	if (result < 0) {
		return -1;
	}
	
	/* Wait for reply */
	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0) {
		return -1;
	}
	
	/* Check server result */
	if (reply.result < 0) {
		return reply.result;
	}
	
	/* Fill tms structure if provided */
	if (tp) {
		tp->tms_utime = reply.tms_utime;
		tp->tms_stime = reply.tms_stime;
		tp->tms_cutime = reply.tms_cutime;
		tp->tms_cstime = reply.tms_cstime;
	}
	
	/* Return elapsed time */
	elapsed = reply.elapsed;
	
	/* Store ticks per second for future reference (optional) */
	if (reply.ticks_per_sec > 0) {
		extern unsigned int _system_ticks_per_sec;
		_system_ticks_per_sec = reply.ticks_per_sec;
	}
	
	return elapsed;
}

/* Public function implementation */
time_t times(struct tms *tp)
{
	return __times(tp, 0, TIMES_SELF | (tp ? TIMES_CHILDREN : 0));
}

/**
 * times_pid - Get times for specific process
 * @tp: Pointer to tms structure
 * @pid: Process ID to query
 *
 * Requires CAP_PROCESS capability for processes other than self.
 */
static inline time_t times_pid(struct tms *tp, pid_t pid)
{
	if (pid <= 0) {
		return -1;  /* EINVAL */
	}
	
	return __times(tp, pid, TIMES_PID | TIMES_CHILDREN);
}

/**
 * times_self - Get times for current process only (no children)
 * @tp: Pointer to tms structure
 */
static inline time_t times_self(struct tms *tp)
{
	return __times(tp, 0, TIMES_SELF);
}

/**
 * times_children - Get accumulated times of all children
 * @tp: Pointer to tms structure
 *
 * Gets and clears the accumulated child times.
 * Useful after wait() to get per-child accounting.
 */
static inline time_t times_children(struct tms *tp)
{
	return __times(tp, 0, TIMES_CHILDREN | TIMES_ACCUM);
}

/**
 * clock - Get processor time used by program
 * 
 * Simplified interface returning CPU time used (user + system)
 * Equivalent to: times(NULL) but returns CPU time instead of elapsed.
 */
static inline clock_t clock(void)
{
	struct tms t;
	time_t elapsed;
	
	elapsed = times(&t);
	if (elapsed == (time_t)-1)
		return (clock_t)-1;
	
	return (clock_t)(t.tms_utime + t.tms_stime);
}

/**
 * sysconf - Get system configuration values
 * @name: Configuration parameter name
 *
 * For _SC_CLK_TCK, returns number of clock ticks per second.
 */
static inline long sysconf(int name)
{
	struct msg_times msg;
	struct msg_times_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	if (name != 2) {  /* _SC_CLK_TCK is usually 2 */
		return -1;
	}
	
	/* Quick query just for ticks per second */
	msg.header.msg_id = MSG_TIMES;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.pid = kernel_state->current_task;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	msg.flags = 0;  /* Just need ticks_per_sec */
	
	if (mk_msg_send(kernel_state->process_server, &msg, sizeof(msg)) < 0)
		return -1;
	
	if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0)
		return -1;
	
	return reply.ticks_per_sec;
}

/**
 * getrusage - Get resource usage
 * @who: RUSAGE_SELF, RUSAGE_CHILDREN
 * @rusage: Resource usage structure
 *
 * Extended interface with more detailed resource information.
 */
struct rusage {
	struct timeval ru_utime;	/* User time used */
	struct timeval ru_stime;	/* System time used */
	long ru_maxrss;			/* Maximum resident set size */
	long ru_ixrss;			/* Integral shared memory size */
	long ru_idrss;			/* Integral unshared data size */
	long ru_isrss;			/* Integral unshared stack size */
	long ru_minflt;			/* Page reclaims */
	long ru_majflt;			/* Page faults */
	long ru_nswap;			/* Swaps */
	long ru_inblock;		/* Block input operations */
	long ru_oublock;		/* Block output operations */
	long ru_msgsnd;			/* Messages sent */
	long ru_msgrcv;			/* Messages received */
	long ru_nsignals;		/* Signals received */
	long ru_nvcsw;			/* Voluntary context switches */
	long ru_nivcsw;			/* Involuntary context switches */
};

#define RUSAGE_SELF	0
#define RUSAGE_CHILDREN	(-1)

static inline int getrusage(int who, struct rusage *rusage)
{
	struct msg_rusage msg;
	struct msg_rusage_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	if (!rusage)
		return -1;
	
	/* Would need new message types for extended info */
	msg.header.msg_id = MSG_GETRUSAGE;  /* Would need definition */
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.who = who;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	
	if (mk_msg_send(kernel_state->process_server, &msg, sizeof(msg)) < 0)
		return -1;
	
	if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0)
		return -1;
	
	memcpy(rusage, &reply.rusage, sizeof(struct rusage));
	return reply.result;
}

/**
 * Constants for time conversion
 */
#define TICKS_PER_SEC_DEFAULT	100	/* Default if not provided */

/* Global ticks per second (cached from server) */
static unsigned int _ticks_per_sec = TICKS_PER_SEC_DEFAULT;

/**
 * tic2sec - Convert ticks to seconds
 */
static inline double tic2sec(time_t ticks)
{
	return (double)ticks / _ticks_per_sec;
}

/**
 * sec2tic - Convert seconds to ticks
 */
static inline time_t sec2tic(double seconds)
{
	return (time_t)(seconds * _ticks_per_sec);
}

/**
 * timeval_to_ticks - Convert timeval to ticks
 */
static inline time_t timeval_to_ticks(struct timeval *tv)
{
	return tv->tv_sec * _ticks_per_sec + 
	       tv->tv_usec * _ticks_per_sec / 1000000;
}

#endif /* _SYS_TIMES_H */
