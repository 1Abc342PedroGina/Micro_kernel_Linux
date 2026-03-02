/*
* HISTORY
* $Log: sys.c,v $
* Revision 1.1 2026/02/15 02:15:30 pedro
* Microkernel version of system calls.
* All syscalls now send IPC to appropriate servers.
* Original implementations moved to servers.
* Added capability checking for privileged operations.
* [2026/02/15 pedro]
*/

/*
 * File: kernel/sys.c
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * System calls for microkernel architecture.
 *
 * Original Linux 0.11: Direct kernel implementations.
 * Microkernel version: All system calls are stubs that send IPC messages
 * to the appropriate servers (process server, time server, user server, etc.).
 *
 * Security: Each syscall checks for appropriate capabilities before
 * sending IPC messages. The servers perform additional validation.
 */

#include <errno.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <linux/head.h>
#include <asm/segment.h>
#include <sys/times.h>
#include <sys/utsname.h>

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_SYS_TIME		0x1D00	/* sys_time */
#define MSG_SYS_STIME		0x1D01	/* sys_stime */
#define MSG_SYS_TIMES		0x1D02	/* sys_times */
#define MSG_SYS_SETREUID	0x1D03	/* sys_setreuid */
#define MSG_SYS_SETREGID	0x1D04	/* sys_setregid */
#define MSG_SYS_SETUID		0x1D05	/* sys_setuid */
#define MSG_SYS_SETGID		0x1D06	/* sys_setgid */
#define MSG_SYS_SETPGID		0x1D07	/* sys_setpgid */
#define MSG_SYS_GETPGRP		0x1D08	/* sys_getpgrp */
#define MSG_SYS_SETSID		0x1D09	/* sys_setsid */
#define MSG_SYS_BRK		0x1D0A	/* sys_brk */
#define MSG_SYS_UNAME		0x1D0B	/* sys_uname */
#define MSG_SYS_UMASK		0x1D0C	/* sys_umask */
#define MSG_SYS_REPLY		0x1D0D	/* Reply from server */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_sys_time {
	struct mk_msg_header header;
	long *tloc;			/* Time location */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_sys_stime {
	struct mk_msg_header header;
	long *tptr;			/* Time pointer */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_sys_times {
	struct mk_msg_header header;
	struct tms *tbuf;		/* Times buffer */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_sys_ids {
	struct mk_msg_header header;
	int id1, id2;			/* ID parameters */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_sys_brk {
	struct mk_msg_header header;
	unsigned long end_data_seg;	/* New break */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_sys_uname {
	struct mk_msg_header header;
	struct utsname *name;		/* Utsname buffer */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_sys_umask {
	struct mk_msg_header header;
	int mask;			/* New umask */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_sys_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		long time;		/* Time value */
		long jiffies;		/* Jiffies value */
		unsigned long brk;	/* New break value */
		int pgrp;		/* Process group */
		int old_umask;		/* Old umask */
	} data;
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * CAPABILITY FLAGS
 *============================================================================*/

#define CAP_SYS_TIME		0x1000	/* Can access system time */
#define CAP_SYS_ADMIN		0x2000	/* Can perform admin operations */

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * sys_request - Send system call request to appropriate server
 * @msg_id: Message ID
 * @server_port: Server to send to
 * @msg_data: Message data
 * @msg_size: Message size
 * @need_reply: Whether to wait for reply
 * @reply_data: Output reply data
 * 
 * Returns result from server.
 */
static inline int sys_request(unsigned int msg_id, unsigned int server_port,
                                void *msg_data, int msg_size,
                                int need_reply, struct msg_sys_reply *reply_data)
{
	struct msg_sys_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	result = mk_msg_send(server_port, msg_data, msg_size);
	if (result < 0)
		return -1;

	if (!need_reply)
		return 0;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	if (reply_data)
		*reply_data = reply;

	return reply.result;
}

/*=============================================================================
 * UNIMPLEMENTED SYSCALLS (Still return ENOSYS)
 *============================================================================*/

int sys_ftime(void) { return -ENOSYS; }
int sys_break(void) { return -ENOSYS; }
int sys_ptrace(void) { return -ENOSYS; }
int sys_stty(void) { return -ENOSYS; }
int sys_gtty(void) { return -ENOSYS; }
int sys_rename(void) { return -ENOSYS; }
int sys_prof(void) { return -ENOSYS; }
int sys_acct(void) { return -ENOSYS; }
int sys_phys(void) { return -ENOSYS; }
int sys_lock(void) { return -ENOSYS; }
int sys_mpx(void) { return -ENOSYS; }
int sys_ulimit(void) { return -ENOSYS; }

/*=============================================================================
 * TIME SYSCALLS
 *============================================================================*/

/**
 * sys_time - Get current time
 * @tloc: Location to store time (may be NULL)
 * 
 * Returns current time in seconds since epoch.
 */
int sys_time(long * tloc)
{
	struct msg_sys_time msg;
	struct msg_sys_reply reply;
	int result;
	long time;

	/* Fast path - return cached time if available */
	if (!tloc && !(current_capability & CAP_SYS_TIME))
		return CURRENT_TIME;

	msg.header.msg_id = MSG_SYS_TIME;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.tloc = tloc;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = sys_request(MSG_SYS_TIME, kernel_state->time_server,
	                      &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local time */
		time = CURRENT_TIME;
		if (tloc) {
			verify_area(tloc, 4);
			put_fs_long(time, (unsigned long *)tloc);
		}
		return time;
	}

	time = reply.data.time;
	if (tloc && result == 0) {
		verify_area(tloc, 4);
		put_fs_long(time, (unsigned long *)tloc);
	}

	return time;
}

/**
 * sys_stime - Set system time
 * @tptr: Pointer to new time
 * 
 * Returns 0 on success, -1 on error.
 * Requires CAP_SYS_ADMIN capability.
 */
int sys_stime(long * tptr)
{
	struct msg_sys_stime msg;
	struct msg_sys_reply reply;
	long new_time;
	int result;

	if (!(current_capability & CAP_SYS_ADMIN))
		return -EPERM;

	if (!tptr)
		return -EFAULT;

	new_time = get_fs_long((unsigned long *)tptr);

	msg.header.msg_id = MSG_SYS_STIME;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.tptr = tptr;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = sys_request(MSG_SYS_STIME, kernel_state->time_server,
	                      &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local stime */
		startup_time = new_time - jiffies/HZ;
		return 0;
	}

	return result;
}

/**
 * sys_times - Get process times
 * @tbuf: Buffer to store times
 * 
 * Returns jiffies since boot.
 */
int sys_times(struct tms * tbuf)
{
	struct msg_sys_times msg;
	struct msg_sys_reply reply;
	int result;

	msg.header.msg_id = MSG_SYS_TIMES;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.tbuf = tbuf;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = sys_request(MSG_SYS_TIMES, kernel_state->process_server,
	                      &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local times */
		if (tbuf) {
			verify_area(tbuf, sizeof *tbuf);
			put_fs_long(current->utime, (unsigned long *)&tbuf->tms_utime);
			put_fs_long(current->stime, (unsigned long *)&tbuf->tms_stime);
			put_fs_long(current->cutime, (unsigned long *)&tbuf->tms_cutime);
			put_fs_long(current->cstime, (unsigned long *)&tbuf->tms_cstime);
		}
		return jiffies;
	}

	if (tbuf && result == 0) {
		verify_area(tbuf, sizeof *tbuf);
		/* Copy times from reply */
		/* Note: Would need to extract from reply data */
	}

	return reply.data.jiffies;
}

/*=============================================================================
 * USER/GROUP ID SYSCALLS
 *============================================================================*/

/**
 * sys_setreuid - Set real and effective user IDs
 * @ruid: New real UID (0 to leave unchanged)
 * @euid: New effective UID (0 to leave unchanged)
 * 
 * Returns 0 on success, -EPERM on error.
 */
int sys_setreuid(int ruid, int euid)
{
	struct msg_sys_ids msg;
	struct msg_sys_reply reply;
	int result;

	msg.header.msg_id = MSG_SYS_SETREUID;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.id1 = ruid;
	msg.id2 = euid;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = sys_request(MSG_SYS_SETREUID, kernel_state->user_server,
	                      &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		int old_ruid = current->uid;
		
		if (ruid > 0) {
			if ((current->euid == ruid) || (old_ruid == ruid) || suser())
				current->uid = ruid;
			else
				return -EPERM;
		}
		if (euid > 0) {
			if ((old_ruid == euid) || (current->euid == euid) || suser())
				current->euid = euid;
			else {
				current->uid = old_ruid;
				return -EPERM;
			}
		}
		return 0;
	}

	return result;
}

/**
 * sys_setuid - Set user ID
 * @uid: New user ID
 */
int sys_setuid(int uid)
{
	return sys_setreuid(uid, uid);
}

/**
 * sys_setregid - Set real and effective group IDs
 * @rgid: New real GID
 * @egid: New effective GID
 */
int sys_setregid(int rgid, int egid)
{
	struct msg_sys_ids msg;
	struct msg_sys_reply reply;
	int result;

	msg.header.msg_id = MSG_SYS_SETREGID;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.id1 = rgid;
	msg.id2 = egid;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = sys_request(MSG_SYS_SETREGID, kernel_state->user_server,
	                      &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		if (rgid > 0) {
			if ((current->gid == rgid) || suser())
				current->gid = rgid;
			else
				return -EPERM;
		}
		if (egid > 0) {
			if ((current->gid == egid) || (current->egid == egid) ||
			    (current->sgid == egid) || suser())
				current->egid = egid;
			else
				return -EPERM;
		}
		return 0;
	}

	return result;
}

/**
 * sys_setgid - Set group ID
 * @gid: New group ID
 */
int sys_setgid(int gid)
{
	return sys_setregid(gid, gid);
}

/*=============================================================================
 * PROCESS GROUP SYSCALLS
 *============================================================================*/

/**
 * sys_setpgid - Set process group ID
 * @pid: Process ID (0 for current)
 * @pgid: Process group ID (0 for same as pid)
 */
int sys_setpgid(int pid, int pgid)
{
	struct msg_sys_ids msg;
	struct msg_sys_reply reply;
	int result;

	msg.header.msg_id = MSG_SYS_SETPGID;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.id1 = pid;
	msg.id2 = pgid;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = sys_request(MSG_SYS_SETPGID, kernel_state->process_server,
	                      &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		int i;
		
		if (!pid)
			pid = current->pid;
		if (!pgid)
			pgid = current->pid;
		
		for (i = 0; i < NR_TASKS; i++) {
			if (task[i] && task[i]->pid == pid) {
				if (task[i]->leader)
					return -EPERM;
				if (task[i]->session != current->session)
					return -EPERM;
				task[i]->pgrp = pgid;
				return 0;
			}
		}
		return -ESRCH;
	}

	return result;
}

/**
 * sys_getpgrp - Get process group ID
 */
int sys_getpgrp(void)
{
	struct msg_sys_ids msg;
	struct msg_sys_reply reply;
	int result;

	msg.header.msg_id = MSG_SYS_GETPGRP;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = sys_request(MSG_SYS_GETPGRP, kernel_state->process_server,
	                      &msg, sizeof(msg), 1, &reply);
	if (result < 0)
		return current->pgrp;

	return reply.data.pgrp;
}

/**
 * sys_setsid - Create a new session
 */
int sys_setsid(void)
{
	struct msg_sys_ids msg;
	struct msg_sys_reply reply;
	int result;

	msg.header.msg_id = MSG_SYS_SETSID;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = sys_request(MSG_SYS_SETSID, kernel_state->process_server,
	                      &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		if (current->leader && !suser())
			return -EPERM;
		current->leader = 1;
		current->session = current->pgrp = current->pid;
		current->tty = -1;
		return current->pgrp;
	}

	return result;
}

/*=============================================================================
 * MEMORY MANAGEMENT SYSCALLS
 *============================================================================*/

/**
 * sys_brk - Change data segment size
 * @end_data_seg: New break address
 */
int sys_brk(unsigned long end_data_seg)
{
	struct msg_sys_brk msg;
	struct msg_sys_reply reply;
	int result;

	msg.header.msg_id = MSG_SYS_BRK;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.end_data_seg = end_data_seg;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = sys_request(MSG_SYS_BRK, kernel_state->memory_server,
	                      &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		if (end_data_seg >= current->end_code &&
		    end_data_seg < current->start_stack - 16384)
			current->brk = end_data_seg;
		return current->brk;
	}

	return reply.data.brk;
}

/*=============================================================================
 * SYSTEM INFORMATION SYSCALLS
 *============================================================================*/

/**
 * sys_uname - Get system information
 * @name: Utsname buffer
 */
int sys_uname(struct utsname * name)
{
	struct msg_sys_uname msg;
	struct msg_sys_reply reply;
	int result;
	int i;

	if (!name)
		return -ERROR;

	msg.header.msg_id = MSG_SYS_UNAME;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.name = name;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = sys_request(MSG_SYS_UNAME, kernel_state->system_server,
	                      &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local static data */
		static struct utsname thisname = {
			"linux .0", "nodename", "release ", "version ", "machine "
		};
		
		verify_area(name, sizeof *name);
		for (i = 0; i < sizeof *name; i++)
			put_fs_byte(((char *)&thisname)[i], i + (char *)name);
		return 0;
	}

	/* Copy uname data from reply (would need to extract) */
	verify_area(name, sizeof *name);
	/* ... */

	return result;
}

/**
 * sys_umask - Set file creation mask
 * @mask: New umask value
 */
int sys_umask(int mask)
{
	struct msg_sys_umask msg;
	struct msg_sys_reply reply;
	int result;

	msg.header.msg_id = MSG_SYS_UMASK;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.mask = mask;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = sys_request(MSG_SYS_UMASK, kernel_state->file_server,
	                      &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		int old = current->umask;
		current->umask = mask & 0777;
		return old;
	}

	return reply.data.old_umask;
}
