/*
* HISTORY
* $Log: fcntl.h,v $
* Revision 1.1 2026/02/14 12:30:45 pedro
* Microkernel version of file control operations.
* open/creat/fcntl now send IPC messages to file server.
* File locking structures preserved for compatibility.
* Added capability checking for all operations.
* [2026/02/14 pedro]
*/

/*
* File: fcntl.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* File control operations for microkernel architecture.
*
* Original Linux 0.11: Direct kernel calls for file operations.
* Microkernel version: All fcntl operations send IPC messages to the file server,
* which manages file descriptors, locks, and capabilities.
*
* Security: File operations require appropriate capabilities:
* - CAP_FILE: Basic file operations
* - CAP_FILE_LOCK: File locking operations
* - CAP_FILE_ADMIN: Administrative operations (F_SETFD, etc.)
*/

#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/types.h>
#include <linux/kernel.h>
#include <linux/head.h>

/*=============================================================================
 * ORIGINAL FLAGS AND COMMANDS (Preserved exactly)
 *============================================================================*/

/* open/fcntl - NOCTTY, NDELAY isn't implemented yet */
#define O_ACCMODE	00003
#define O_RDONLY	   00
#define O_WRONLY	   01
#define O_RDWR		   02
#define O_CREAT		00100	/* not fcntl */
#define O_EXCL		00200	/* not fcntl */
#define O_NOCTTY	00400	/* not fcntl */
#define O_TRUNC		01000	/* not fcntl */
#define O_APPEND	02000
#define O_NONBLOCK	04000	/* not fcntl */
#define O_NDELAY	O_NONBLOCK

/* Defines for fcntl-commands. Note that currently
 * locking isn't supported, and other things aren't really
 * tested.
 */
#define F_DUPFD		0	/* dup */
#define F_GETFD		1	/* get f_flags */
#define F_SETFD		2	/* set f_flags */
#define F_GETFL		3	/* more flags (cloexec) */
#define F_SETFL		4
#define F_GETLK		5	/* not implemented */
#define F_SETLK		6
#define F_SETLKW	7

/* for F_[GET|SET]FL */
#define FD_CLOEXEC	1	/* actually anything with low bit set goes */

/* Ok, these are locking features, and aren't implemented at any
 * level. POSIX wants them.
 */
#define F_RDLCK		0
#define F_WRLCK		1
#define F_UNLCK		2

/* Once again - not implemented, but ... */
struct flock {
	short l_type;
	short l_whence;
	off_t l_start;
	off_t l_len;
	pid_t l_pid;
};

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_FCNTL_OPEN		0x1000	/* Open file */
#define MSG_FCNTL_CREAT		0x1001	/* Create file */
#define MSG_FCNTL_DUPFD		0x1002	/* Duplicate file descriptor */
#define MSG_FCNTL_GETFD		0x1003	/* Get file descriptor flags */
#define MSG_FCNTL_SETFD		0x1004	/* Set file descriptor flags */
#define MSG_FCNTL_GETFL		0x1005	/* Get file status flags */
#define MSG_FCNTL_SETFL		0x1006	/* Set file status flags */
#define MSG_FCNTL_GETLK		0x1007	/* Get lock information */
#define MSG_FCNTL_SETLK		0x1008	/* Set lock (non-blocking) */
#define MSG_FCNTL_SETLKW	0x1009	/* Set lock (blocking) */
#define MSG_FCNTL_REPLY		0x100A	/* Reply from file server */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_fcntl_open {
	struct mk_msg_header header;
	const char *filename;		/* File to open */
	int flags;			/* Open flags */
	mode_t mode;			/* Creation mode (if O_CREAT) */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_fcntl_creat {
	struct mk_msg_header header;
	const char *filename;		/* File to create */
	mode_t mode;			/* Creation mode */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_fcntl_dupfd {
	struct mk_msg_header header;
	int fildes;			/* File descriptor to duplicate */
	int minfd;			/* Minimum new descriptor */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_fcntl_getsetfd {
	struct mk_msg_header header;
	int fildes;			/* File descriptor */
	int flags;			/* Flags to set (for SET) */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_fcntl_getsetfl {
	struct mk_msg_header header;
	int fildes;			/* File descriptor */
	int flags;			/* Flags to set (for SET) */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_fcntl_lock {
	struct mk_msg_header header;
	int fildes;			/* File descriptor */
	int cmd;			/* F_GETLK, F_SETLK, F_SETLKW */
	struct flock lock;		/* Lock information */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_fcntl_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		int fd;			/* File descriptor (for open/creat/dup) */
		int flags;		/* Flags (for GETFD/GETFL) */
		struct flock lock;	/* Lock information (for GETLK) */
	} data;
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * CAPABILITY FLAGS
 *============================================================================*/

#define CAP_FILE_OPEN		0x0100	/* Can open files */
#define CAP_FILE_CREAT		0x0200	/* Can create files */
#define CAP_FILE_DUP		0x0400	/* Can duplicate descriptors */
#define CAP_FILE_FLAGS		0x0800	/* Can get/set file flags */
#define CAP_FILE_LOCK		0x1000	/* Can lock files */
#define CAP_FILE_ADMIN		0x2000	/* Administrative operations */

/*=============================================================================
 * PUBLIC FUNCTION PROTOTYPES
 *============================================================================*/

extern int creat(const char * filename, mode_t mode);
extern int fcntl(int fildes, int cmd, ...);
extern int open(const char * filename, int flags, ...);

/*=============================================================================
 * INTERNAL HELPER FUNCTIONS
 *============================================================================*/

/**
 * __fcntl_request - Send fcntl request to file server
 * @cmd: Fcntl command
 * @fildes: File descriptor
 * @arg: Command argument
 * @msg_id: IPC message ID
 * @need_reply: Whether to wait for reply
 * 
 * Returns command-specific result, or -1 on error.
 */
static inline long __fcntl_request(int cmd, int fildes, unsigned long arg,
                                     unsigned int msg_id, int need_reply)
{
	struct msg_fcntl_getsetfd msg;
	struct msg_fcntl_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	/* Check basic file capability */
	if (!(current_capability & CAP_FILE)) {
		if (request_file_capability() < 0)
			return -1;
	}

	/* Prepare message */
	msg.header.msg_id = msg_id;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = need_reply ? kernel_state->kernel_port : 0;
	msg.header.size = sizeof(msg);

	msg.fildes = fildes;
	msg.flags = (int)arg;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	/* Send request */
	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	/* Wait for reply if needed */
	if (!need_reply)
		return 0;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	/* Update capabilities if granted */
	if (reply.granted_caps)
		current_capability |= reply.granted_caps;

	return (reply.result < 0) ? reply.result : reply.data.flags;
}

/*=============================================================================
 * PUBLIC FUNCTION IMPLEMENTATIONS
 *============================================================================*/

/**
 * open - Open or create a file
 * @filename: Path to file
 * @flags: Open flags (O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, etc.)
 * @mode: File mode (only used when O_CREAT is set)
 * 
 * Opens a file and returns a file descriptor, or -1 on error.
 * 
 * In microkernel mode, this sends an IPC message to the file server,
 * which validates capabilities and returns a file descriptor capability.
 */
extern int open(const char * filename, int flags, ...)
{
	struct msg_fcntl_open msg;
	struct msg_fcntl_reply reply;
	unsigned int reply_size = sizeof(reply);
	mode_t mode = 0;
	int result;

	/* Get mode if O_CREAT is specified */
	if (flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}

	/* Check specific capability */
	if (!(current_capability & CAP_FILE_OPEN)) {
		if (request_file_capability() < 0)
			return -1;
	}

	/* Prepare message */
	msg.header.msg_id = MSG_FCNTL_OPEN;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.filename = filename;
	msg.flags = flags;
	msg.mode = mode;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	/* Send request */
	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	/* Wait for reply */
	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	/* Update capabilities if granted */
	if (reply.granted_caps)
		current_capability |= reply.granted_caps;

	return (reply.result < 0) ? reply.result : reply.data.fd;
}

/**
 * creat - Create a file (equivalent to open with O_CREAT|O_WRONLY|O_TRUNC)
 * @filename: Path to file
 * @mode: File mode
 * 
 * Creates a new file or truncates an existing one, opening it for writing.
 * Returns a file descriptor, or -1 on error.
 */
extern int creat(const char * filename, mode_t mode)
{
	struct msg_fcntl_creat msg;
	struct msg_fcntl_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_FILE_CREAT)) {
		if (request_file_capability() < 0)
			return -1;
	}

	msg.header.msg_id = MSG_FCNTL_CREAT;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.filename = filename;
	msg.mode = mode;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return (reply.result < 0) ? reply.result : reply.data.fd;
}

/**
 * fcntl - Manipulate file descriptor
 * @fildes: File descriptor
 * @cmd: Command (F_DUPFD, F_GETFD, F_SETFD, F_GETFL, F_SETFL, F_GETLK, F_SETLK, F_SETLKW)
 * @...: Command-specific argument
 * 
 * Performs various operations on file descriptors.
 * Returns command-specific result, or -1 on error.
 */
extern int fcntl(int fildes, int cmd, ...)
{
	va_list ap;
	unsigned long arg = 0;
	int result;

	/* Get optional argument */
	va_start(ap, cmd);
	arg = va_arg(ap, unsigned long);
	va_end(ap);

	switch (cmd) {
		case F_DUPFD:
			if (!(current_capability & CAP_FILE_DUP))
				return -1;
			/* Special handling for dup */
			{
				struct msg_fcntl_dupfd msg;
				struct msg_fcntl_reply reply;
				unsigned int reply_size = sizeof(reply);

				msg.header.msg_id = MSG_FCNTL_DUPFD;
				msg.header.sender_port = kernel_state->kernel_port;
				msg.header.reply_port = kernel_state->kernel_port;
				msg.header.size = sizeof(msg);

				msg.fildes = fildes;
				msg.minfd = (int)arg;
				msg.task_id = kernel_state->current_task;
				msg.caps = current_capability;

				if (mk_msg_send(kernel_state->file_server, &msg, sizeof(msg)) < 0)
					return -1;

				if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0)
					return -1;

				return (reply.result < 0) ? reply.result : reply.data.fd;
			}

		case F_GETFD:
		case F_SETFD:
			if (!(current_capability & CAP_FILE_ADMIN))
				return -1;
			return __fcntl_request(cmd, fildes, arg,
				(cmd == F_GETFD) ? MSG_FCNTL_GETFD : MSG_FCNTL_SETFD, 1);

		case F_GETFL:
		case F_SETFL:
			if (!(current_capability & CAP_FILE_FLAGS))
				return -1;
			return __fcntl_request(cmd, fildes, arg,
				(cmd == F_GETFL) ? MSG_FCNTL_GETFL : MSG_FCNTL_SETFL, 1);

		case F_GETLK:
		case F_SETLK:
		case F_SETLKW:
			if (!(current_capability & CAP_FILE_LOCK))
				return -1;
			/* Lock operations need special handling */
			{
				struct msg_fcntl_lock msg;
				struct msg_fcntl_reply reply;
				unsigned int reply_size = sizeof(reply);
				struct flock *lock = (struct flock *)arg;

				msg.header.msg_id = (cmd == F_GETLK) ? MSG_FCNTL_GETLK :
				                          (cmd == F_SETLK) ? MSG_FCNTL_SETLK :
				                          MSG_FCNTL_SETLKW;
				msg.header.sender_port = kernel_state->kernel_port;
				msg.header.reply_port = kernel_state->kernel_port;
				msg.header.size = sizeof(msg);

				msg.fildes = fildes;
				msg.cmd = cmd;
				if (lock)
					msg.lock = *lock;
				msg.task_id = kernel_state->current_task;
				msg.caps = current_capability;

				if (mk_msg_send(kernel_state->file_server, &msg, sizeof(msg)) < 0)
					return -1;

				if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0)
					return -1;

				if (reply.result == 0 && cmd == F_GETLK && lock)
					*lock = reply.data.lock;

				return reply.result;
			}

		default:
			return -1;  /* EINVAL */
	}
}

/*=============================================================================
 * CONVENIENCE FUNCTIONS
 *============================================================================*/

/**
 * dup - Duplicate a file descriptor
 * @fildes: File descriptor to duplicate
 * 
 * Equivalent to fcntl(fildes, F_DUPFD, 0).
 */
static inline int dup(int fildes)
{
	return fcntl(fildes, F_DUPFD, 0);
}

/**
 * dup2 - Duplicate a file descriptor to a specific number
 * @oldfd: File descriptor to duplicate
 * @newfd: Desired new file descriptor number
 * 
 * Closes newfd if it's open, then duplicates oldfd to newfd.
 */
static inline int dup2(int oldfd, int newfd)
{
	int result;

	/* Close newfd if it's open (ignore errors) */
	close(newfd);

	/* Try to duplicate to the exact number */
	result = fcntl(oldfd, F_DUPFD, newfd);
	
	/* If we got a different number, something went wrong */
	if (result != newfd) {
		if (result >= 0)
			close(result);
		return -1;
	}
	
	return result;
}

/**
 * fcntl_getfd - Get file descriptor flags
 * @fd: File descriptor
 */
static inline int fcntl_getfd(int fd)
{
	return fcntl(fd, F_GETFD, 0);
}

/**
 * fcntl_setfd - Set file descriptor flags
 * @fd: File descriptor
 * @flags: Flags to set (FD_CLOEXEC, etc.)
 */
static inline int fcntl_setfd(int fd, int flags)
{
	return fcntl(fd, F_SETFD, flags);
}

/**
 * fcntl_getfl - Get file status flags
 * @fd: File descriptor
 */
static inline int fcntl_getfl(int fd)
{
	return fcntl(fd, F_GETFL, 0);
}

/**
 * fcntl_setfl - Set file status flags
 * @fd: File descriptor
 * @flags: Flags to set (O_APPEND, O_NONBLOCK, etc.)
 */
static inline int fcntl_setfl(int fd, int flags)
{
	return fcntl(fd, F_SETFL, flags);
}

/*=============================================================================
 * CAPABILITY REQUEST FUNCTION
 *============================================================================*/

/**
 * request_file_capability - Request file capability
 * @cap: Specific capability to request (CAP_FILE_*)
 * 
 * Requests file-related capabilities from the file server.
 * Returns 0 on success, -1 on error.
 */
static inline int request_file_capability(unsigned int cap)
{
	struct msg_cap_request msg;
	struct msg_cap_reply reply;
	unsigned int reply_size = sizeof(reply);

	msg.header.msg_id = MSG_CAP_REQUEST_FILE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.task_id = kernel_state->current_task;
	msg.requested_cap = cap;

	if (mk_msg_send(kernel_state->file_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0) {
				current_capability |= cap;
				return 0;
			}
		}
	}
	return -1;
}

#endif /* _FCNTL_H */
