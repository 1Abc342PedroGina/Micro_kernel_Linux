/*
* HISTORY
* $Log: stat.h,v $
* Revision 1.1 2026/02/14 02:45:30 pedro
* Microkernel version of file status operations.
* stat/fstat/chmod/mkdir/mkfifo/umask now send IPC messages to file server.
* struct stat preserved for compatibility, but fields now represent
* capability-aware concepts.
* [2026/02/14 pedro]
*/

/*
* File: sys/stat.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* File status operations for microkernel architecture.
*
* Original Linux 0.11: Direct kernel calls to file system.
* Microkernel version: All stat operations send IPC messages to the file server,
* which manages file capabilities and metadata.
*
* struct stat fields now have microkernel meanings:
* - st_dev: Device server port + capability domain
* - st_ino: Memory object capability ID
* - st_mode: File type + POSIX permissions + capability flags
* - st_nlink: Capability reference count
* - st_uid/st_gid: Capability domain IDs
* - st_rdev: Device server port (for special files)
* - st_size: Size in bytes (validated by capability bounds)
*/

#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>
#include <linux/kernel.h>
#include <linux/head.h>

/* Message codes for stat operations */
#define MSG_STAT		0x0B00	/* Get file status by path */
#define MSG_FSTAT		0x0B01	/* Get file status by fd */
#define MSG_CHMOD		0x0B02	/* Change file mode */
#define MSG_MKDIR		0x0B03	/* Create directory */
#define MSG_MKFIFO		0x0B04	/* Create FIFO */
#define MSG_UMASK		0x0B05	/* Set file creation mask */
#define MSG_STAT_REPLY		0x0B06	/* Reply with stat data */

/* Stat operation message structures */
struct msg_stat {
	struct mk_msg_header header;
	char *path;			/* Path to file */
	struct stat *stat_buf;		/* Buffer for result (in user space) */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
	unsigned int flags;		/* Operation flags */
};

struct msg_fstat {
	struct mk_msg_header header;
	int fd;				/* File descriptor */
	struct stat *stat_buf;		/* Buffer for result */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_chmod {
	struct mk_msg_header header;
	char *path;			/* Path to file */
	mode_t mode;			/* New mode */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_mkdir {
	struct mk_msg_header header;
	char *path;			/* Path to create */
	mode_t mode;			/* Creation mode */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_mkfifo {
	struct mk_msg_header header;
	char *path;			/* Path to create */
	mode_t mode;			/* Creation mode */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_umask {
	struct mk_msg_header header;
	mode_t mask;			/* New umask */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_stat_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	struct stat st;			/* Stat data */
	capability_t file_caps;		/* File capabilities granted */
};

/*
 * struct stat - File status structure
 * Complete o mesmo que o original para compatibilidade binária
 */
struct stat {
	dev_t	st_dev;		/* Device ID (server port + domain) */
	ino_t	st_ino;		/* Inode number (capability ID) */
	umode_t	st_mode;	/* File mode (type + perms + caps) */
	nlink_t	st_nlink;	/* Number of links (capability refcount) */
	uid_t	st_uid;		/* User ID (capability domain) */
	gid_t	st_gid;		/* Group ID (secondary domain) */
	dev_t	st_rdev;	/* Device ID for special files */
	off_t	st_size;	/* File size in bytes */
	time_t	st_atime;	/* Time of last access */
	time_t	st_mtime;	/* Time of last modification */
	time_t	st_ctime;	/* Time of last status change */
};

/* File type masks (original) */
#define S_IFMT  00170000	/* Type of file mask */
#define S_IFREG 0100000		/* Regular file */
#define S_IFBLK 0060000		/* Block special */
#define S_IFDIR 0040000		/* Directory */
#define S_IFCHR 0020000		/* Character special */
#define S_IFIFO 0010000		/* FIFO */

/* File mode bits (original) */
#define S_ISUID 0004000		/* Set user ID on execution */
#define S_ISGID 0002000		/* Set group ID on execution */
#define S_ISVTX 0001000		/* Sticky bit */

/* File type test macros (original) */
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)

/* Permission bits (original) */
#define S_IRWXU 00700		/* Read, write, execute by owner */
#define S_IRUSR 00400		/* Read by owner */
#define S_IWUSR 00200		/* Write by owner */
#define S_IXUSR 00100		/* Execute by owner */

#define S_IRWXG 00070		/* Read, write, execute by group */
#define S_IRGRP 00040		/* Read by group */
#define S_IWGRP 00020		/* Write by group */
#define S_IXGRP 00010		/* Execute by group */

#define S_IRWXO 00007		/* Read, write, execute by others */
#define S_IROTH 00004		/* Read by others */
#define S_IWOTH 00002		/* Write by others */
#define S_IXOTH 00001		/* Execute by others */

/* Microkernel capability flags (extended mode bits) */
#define S_IFCAP  0x0000F000	/* Capability flags mask (upper bits) */
#define S_ICAP_READ  0x00001000	/* File grants read capability */
#define S_ICAP_WRITE 0x00002000	/* File grants write capability */
#define S_ICAP_EXEC  0x00004000	/* File grants execute capability */
#define S_ICAP_SHARE 0x00008000	/* File is shareable */
#define S_ICAP_COW   0x00010000	/* Copy-on-write access */

/* Convert between POSIX mode and file capabilities */
#define MODE_TO_FCAPS(mode) ((((mode) & S_IRUSR) ? S_ICAP_READ : 0) | \
                              (((mode) & S_IWUSR) ? S_ICAP_WRITE : 0) | \
                              (((mode) & S_IXUSR) ? S_ICAP_EXEC : 0))

#define FCAPS_TO_MODE(caps) ((((caps) & S_ICAP_READ) ? S_IRUSR : 0) | \
                              (((caps) & S_ICAP_WRITE) ? S_IWUSR : 0) | \
                              (((caps) & S_ICAP_EXEC) ? S_IXUSR : 0))

/**
 * stat - Get file status by path
 * @filename: Path to file
 * @stat_buf: Buffer to store status information
 *
 * Returns 0 on success, -1 on error.
 */
extern int stat(const char *filename, struct stat *stat_buf);

/**
 * fstat - Get file status by file descriptor
 * @fildes: File descriptor
 * @stat_buf: Buffer to store status information
 *
 * Returns 0 on success, -1 on error.
 */
extern int fstat(int fildes, struct stat *stat_buf);

/**
 * chmod - Change file mode
 * @_path: Path to file
 * @mode: New file mode
 *
 * Returns 0 on success, -1 on error.
 */
extern int chmod(const char *_path, mode_t mode);

/**
 * mkdir - Create a directory
 * @_path: Path to create
 * @mode: Directory mode
 *
 * Returns 0 on success, -1 on error.
 */
extern int mkdir(const char *_path, mode_t mode);

/**
 * mkfifo - Create a FIFO (named pipe)
 * @_path: Path to create
 * @mode: FIFO mode
 *
 * Returns 0 on success, -1 on error.
 */
extern int mkfifo(const char *_path, mode_t mode);

/**
 * umask - Set file creation mask
 * @mask: New mask value
 *
 * Returns the previous mask value.
 */
extern mode_t umask(mode_t mask);

/* Internal implementations as inline functions */

static inline int __do_stat(const char *path, struct stat *stat_buf, int is_fstat, int fd)
{
	struct msg_stat msg;
	struct msg_fstat fmsg;
	struct msg_stat_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;
	void *msg_ptr;
	unsigned int msg_size;

	/* Validate arguments */
	if (!stat_buf) {
		return -1;  /* EFAULT */
	}

	/* Check file capability */
	if (!(current_capability & CAP_FILE)) {
		if (request_file_capability() < 0)
			return -1;  /* EPERM */
	}

	if (is_fstat) {
		/* fstat message */
		fmsg.header.msg_id = MSG_FSTAT;
		fmsg.header.sender_port = kernel_state->kernel_port;
		fmsg.header.reply_port = kernel_state->kernel_port;
		fmsg.header.size = sizeof(fmsg);
		
		fmsg.fd = fd;
		fmsg.stat_buf = stat_buf;
		fmsg.task_id = kernel_state->current_task;
		fmsg.caps = current_capability;
		
		msg_ptr = &fmsg;
		msg_size = sizeof(fmsg);
	} else {
		/* stat message */
		msg.header.msg_id = MSG_STAT;
		msg.header.sender_port = kernel_state->kernel_port;
		msg.header.reply_port = kernel_state->kernel_port;
		msg.header.size = sizeof(msg);
		
		msg.path = (char *)path;
		msg.stat_buf = stat_buf;
		msg.task_id = kernel_state->current_task;
		msg.caps = current_capability;
		msg.flags = 0;
		
		msg_ptr = &msg;
		msg_size = sizeof(msg);
	}

	/* Send to file server */
	result = mk_msg_send(kernel_state->file_server, msg_ptr, msg_size);
	if (result < 0) {
		return -1;
	}

	/* Wait for reply */
	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0) {
		return -1;
	}

	/* Copy stat data to user buffer */
	if (reply.result == 0) {
		memcpy(stat_buf, &reply.st, sizeof(struct stat));
		
		/* Update capabilities if file grants any */
		if (reply.file_caps) {
			current_capability |= reply.file_caps;
		}
	}

	return reply.result;
}

/* Public function implementations */
int stat(const char *filename, struct stat *stat_buf)
{
	if (!filename) {
		return -1;  /* ENOENT */
	}
	return __do_stat(filename, stat_buf, 0, 0);
}

int fstat(int fildes, struct stat *stat_buf)
{
	if (fildes < 0) {
		return -1;  /* EBADF */
	}
	return __do_stat(NULL, stat_buf, 1, fildes);
}

int chmod(const char *_path, mode_t mode)
{
	struct msg_chmod msg;
	struct msg_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!_path) {
		return -1;
	}

	if (!(current_capability & CAP_FILE)) {
		return -1;  /* EPERM */
	}

	msg.header.msg_id = MSG_CHMOD;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.path = (char *)_path;
	msg.mode = mode;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0) {
		return -1;
	}

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0) {
		return -1;
	}

	return reply.result;
}

int mkdir(const char *_path, mode_t mode)
{
	struct msg_mkdir msg;
	struct msg_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!_path) {
		return -1;
	}

	if (!(current_capability & CAP_FILE)) {
		return -1;
	}

	msg.header.msg_id = MSG_MKDIR;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.path = (char *)_path;
	msg.mode = mode;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0) {
		return -1;
	}

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0) {
		return -1;
	}

	return reply.result;
}

int mkfifo(const char *_path, mode_t mode)
{
	struct msg_mkfifo msg;
	struct msg_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!_path) {
		return -1;
	}

	if (!(current_capability & CAP_FILE)) {
		return -1;
	}

	msg.header.msg_id = MSG_MKFIFO;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.path = (char *)_path;
	msg.mode = mode;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0) {
		return -1;
	}

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0) {
		return -1;
	}

	return reply.result;
}

mode_t umask(mode_t mask)
{
	struct msg_umask msg;
	struct msg_umask_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;
	static mode_t current_umask = 022;  /* Default umask */

	/* If no file server available yet, use local */
	if (!kernel_state->file_server) {
		mode_t old = current_umask;
		current_umask = mask & 0777;
		return old;
	}

	msg.header.msg_id = MSG_UMASK;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.mask = mask;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0) {
		return current_umask;  /* Fallback to local */
	}

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0) {
		return current_umask;
	}

	return reply.old_mask;
}

/**
 * request_file_capability - Request file system capability
 */
static inline int request_file_capability(void)
{
	struct msg_cap_request msg;
	struct msg_cap_reply reply;
	unsigned int reply_size = sizeof(reply);

	msg.header.msg_id = MSG_CAP_REQUEST_FILE;  /* Would need definition */
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.task_id = kernel_state->current_task;
	msg.requested_cap = CAP_FILE;

	if (mk_msg_send(kernel_state->file_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0) {
				current_capability |= CAP_FILE;
				return 0;
			}
		}
	}
	return -1;
}

/**
 * chown - Change file owner (requires CAP_SYSTEM)
 * @path: Path to file
 * @owner: New owner (capability domain)
 * @group: New group (capability domain)
 */
static inline int chown(const char *path, uid_t owner, gid_t group)
{
	/* Would need MSG_CHOWN implementation */
	/* Only tasks with CAP_SYSTEM can change ownership */
	if (!(current_capability & CAP_SYSTEM)) {
		return -1;  /* EPERM */
	}
	return -1;  /* ENOSYS */
}

/**
 * lstat - Get status of symbolic link (not in original Linux 0.11)
 * For compatibility with newer programs.
 */
static inline int lstat(const char *path, struct stat *stat_buf)
{
	/* In Linux 0.11, symlinks don't exist */
	return stat(path, stat_buf);
}

/**
 * mknod - Create a special file
 * @path: Path to create
 * @mode: File mode (including type)
 * @dev: Device ID
 */
static inline int mknod(const char *path, mode_t mode, dev_t dev)
{
	/* Would need MSG_MKNOD implementation */
	return -1;  /* ENOSYS */
}

#endif /* _SYS_STAT_H */
