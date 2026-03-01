/*
* HISTORY
* $Log: utime.h,v $
* Revision 1.1 2026/02/14 04:15:30 pedro
* Microkernel version of utime operation.
* utime() now sends IPC message to file server.
* struct utimbuf preserved for compatibility.
* Added capability checking for timestamp modification.
* [2026/02/14 pedro]
*/

/*
* File: utime.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* File timestamp operations for microkernel architecture.
*
* Original Linux 0.11: Direct kernel call to update file times.
* Microkernel version: utime() sends IPC message to file server,
* which validates capabilities and updates file metadata.
*
* Security: Changing file timestamps requires:
* - Write access to the file (CAP_WRITE or file write permission)
* - Or being owner of the file (same capability domain)
* - Or CAP_SYSTEM capability
*/

#ifndef _UTIME_H
#define _UTIME_H

#include <sys/types.h>
#include <linux/kernel.h>
#include <linux/head.h>

/* Message codes for utime operation */
#define MSG_UTIME		0x0D00	/* Update file times */
#define MSG_UTIME_REPLY		0x0D01	/* Reply with result */

/* utime message structures */
struct msg_utime {
	struct mk_msg_header header;
	char *filename;			/* Path to file */
	struct utimbuf *times;		/* New timestamps (or NULL for current) */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
	unsigned int flags;		/* Operation flags */
};

struct msg_utime_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	time_t old_atime;		/* Previous access time */
	time_t old_mtime;		/* Previous modify time */
	capability_t granted_caps;	/* New capabilities (if any) */
};

/*
 * utimbuf structure -完全相同 ao original para compatibilidade
 * 
 * Original Linux 0.11: 
 * - actime: access time (last read)
 * - modtime: modification time (last write)
 * 
 * Microkernel version: Same structure, but times are now managed
 * by the file server and may be virtualized per capability domain.
 */
struct utimbuf {
	time_t actime;		/* Access time (seconds since epoch) */
	time_t modtime;		/* Modification time */
};

/* Flags for utime operation */
#define UTIME_NONE	0x00	/* No special handling */
#define UTIME_NOW	0x01	/* Set to current time (times == NULL) */
#define UTIME_OMIT	0x02	/* Omit this timestamp (for futimens) */
#define UTIME_SYNC	0x04	/* Synchronous update (wait for disk) */
#define UTIME_CAP_CHECK	0x08	/* Force capability check */

/**
 * utime - Change file access and modification times
 * @filename: Path to file
 * @times: Pointer to utimbuf structure with new times, or NULL to set to current
 *
 * Updates the access and modification times of the specified file.
 * If times is NULL, both timestamps are set to the current time.
 * 
 * Returns 0 on success, -1 on error.
 */
extern int utime(const char *filename, struct utimbuf *times);

/* Internal implementation as inline function */
static inline int __do_utime(const char *filename, struct utimbuf *times, 
                              unsigned int flags)
{
	struct msg_utime msg;
	struct msg_utime_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	/* Validate arguments */
	if (!filename) {
		return -1;  /* ENOENT */
	}

	/* Check file capability */
	if (!(current_capability & CAP_FILE)) {
		if (request_file_capability() < 0) {
			/* Try with system capability instead */
			if (!(current_capability & CAP_SYSTEM))
				return -1;  /* EPERM */
		}
	}

	/* Prepare utime message */
	msg.header.msg_id = MSG_UTIME;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.filename = (char *)filename;
	msg.times = times;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	msg.flags = flags;

	/* Send to file server */
	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0) {
		return -1;
	}

	/* Wait for reply */
	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0) {
		return -1;
	}

	/* Update capabilities if file server granted any */
	if (reply.granted_caps) {
		current_capability |= reply.granted_caps;
	}

	return reply.result;
}

/* Public function implementation */
int utime(const char *filename, struct utimbuf *times)
{
	unsigned int flags = (times == NULL) ? UTIME_NOW : UTIME_NONE;
	return __do_utime(filename, times, flags);
}

/**
 * utimes - Set file times with microsecond precision (BSD)
 * @filename: Path to file
 * @times: Array of two timeval structures (access, modification)
 *
 * Extended version with higher precision. Not in original Linux 0.11,
 * but included for completeness.
 */
struct timeval {
	long tv_sec;		/* Seconds */
	long tv_usec;		/* Microseconds */
};

static inline int utimes(const char *filename, const struct timeval times[2])
{
	struct utimbuf ut;
	
	if (times) {
		ut.actime = times[0].tv_sec;
		ut.modtime = times[1].tv_sec;
		return utime(filename, &ut);
	}
	
	return utime(filename, NULL);
}

/**
 * futime - Change file times by file descriptor
 * @fd: File descriptor
 * @times: New timestamps (or NULL for current)
 *
 * Same as utime but using an already open file descriptor.
 */
static inline int futime(int fd, struct utimbuf *times)
{
	struct msg_futime msg;  /* Would need new message type */
	/* Implementation would be similar to utime but with fd */
	return -1;  /* ENOSYS */
}

/**
 * lutime - Change file times without following symlinks
 * @filename: Path to file (doesn't follow symlinks)
 * @times: New timestamps
 *
 * Same as utime but operates on symlink itself, not its target.
 * (Symlinks not in original Linux 0.11, included for completeness)
 */
static inline int lutime(const char *filename, struct utimbuf *times)
{
	/* Would need L_UTIME message type */
	return utime(filename, times);  /* Fallback */
}

/**
 * touch - Create file or update timestamps (like Unix touch command)
 * @filename: Path to file
 *
 * Convenience function: creates file if it doesn't exist,
 * otherwise updates timestamps to current time.
 */
static inline int touch(const char *filename)
{
	int result;
	
	/* Try utime first */
	result = utime(filename, NULL);
	if (result == 0) {
		return 0;  /* File exists, timestamps updated */
	}
	
	/* File doesn't exist, try to create it */
	/* Would need open() with O_CREAT */
	return -1;  /* For now, just fail */
}

/**
 * utime_now - Set file times to current time
 * @filename: Path to file
 *
 * Equivalent to utime(filename, NULL)
 */
static inline int utime_now(const char *filename)
{
	return utime(filename, NULL);
}

/**
 * utime_fixed - Set file times to fixed values
 * @filename: Path to file
 * @atime: New access time
 * @mtime: New modification time
 *
 * Convenience function for setting both times.
 */
static inline int utime_fixed(const char *filename, time_t atime, time_t mtime)
{
	struct utimbuf ut;
	ut.actime = atime;
	ut.modtime = mtime;
	return utime(filename, &ut);
}

/**
 * utime_copy - Copy timestamps from one file to another
 * @src: Source file
 * @dst: Destination file
 *
 * Reads timestamps from source and applies them to destination.
 * Requires read access to source and write access to destination.
 */
static inline int utime_copy(const char *src, const char *dst)
{
	struct stat st;
	struct utimbuf ut;
	int result;
	
	/* Get source timestamps */
	result = stat(src, &st);
	if (result < 0) {
		return -1;
	}
	
	/* Apply to destination */
	ut.actime = st.st_atime;
	ut.modtime = st.st_mtime;
	return utime(dst, &ut);
}

/**
 * utime_touch - Touch multiple files
 * @filev: Array of filenames
 * @count: Number of files
 *
 * Updates timestamps for multiple files.
 */
static inline int utime_touch(const char **filev, int count)
{
	int i;
	int result = 0;
	
	for (i = 0; i < count; i++) {
		if (utime(filev[i], NULL) < 0) {
			result = -1;
		}
	}
	
	return result;
}

/**
 * Time manipulation utilities
 */
#define SECONDS_PER_DAY		(24 * 60 * 60)
#define SECONDS_PER_YEAR	(365 * SECONDS_PER_DAY)

/**
 * utime_relative - Set file times relative to current
 * @filename: Path to file
 * @atime_offset: Offset in seconds for access time (negative = past)
 * @mtime_offset: Offset in seconds for modification time
 *
 * Sets timestamps to current time plus offsets.
 */
static inline int utime_relative(const char *filename, 
                                   long atime_offset, long mtime_offset)
{
	struct utimbuf ut;
	time_t now;
	
	/* Would need time() from time server */
	now = 0;  /* Placeholder */
	
	ut.actime = now + atime_offset;
	ut.modtime = now + mtime_offset;
	return utime(filename, &ut);
}

/**
 * utime_freeze - Set file times to a fixed point in the past
 * @filename: Path to file
 * @days_ago: Number of days ago
 *
 * Useful for testing and archival purposes.
 */
static inline int utime_freeze(const char *filename, int days_ago)
{
	time_t now;
	time_t past;
	
	/* Would need time() */
	now = 0;
	past = now - (days_ago * SECONDS_PER_DAY);
	
	return utime_fixed(filename, past, past);
}

/**
 * request_file_capability - Request file system capability
 * (Reused from stat.h)
 */
static inline int request_file_capability(void)
{
	struct msg_cap_request msg;
	struct msg_cap_reply reply;
	unsigned int reply_size = sizeof(reply);

	msg.header.msg_id = MSG_CAP_REQUEST_FILE;
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

#endif /* _UTIME_H */
