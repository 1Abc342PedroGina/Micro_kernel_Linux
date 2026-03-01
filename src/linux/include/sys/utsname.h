/*
* HISTORY
* $Log: utsname.h,v $
* Revision 1.1 2026/02/14 01:30:45 pedro
* Microkernel version of system information.
* uname() now sends IPC message to system server.
* utsname structure preserved for compatibility.
* Added capability checking for system information.
* [2026/02/14 pedro]
*/

/*
* File: sys/utsname.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* System name information for microkernel architecture.
*
* Original Linux 0.11: Direct kernel data access.
* Microkernel version: uname() sends IPC message to system server,
* which maintains system identity information. The system server
* may have different identities per capability domain
* (containers, virtualization, etc.).
*
* Security: Basic system information (sysname, release) is available
* to all tasks. Nodename and version may require CAP_SYSTEM capability.
*/

#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

#include <sys/types.h>
#include <linux/kernel.h>  /* For kernel_state, mk_msg_send/receive */
#include <linux/head.h>    /* For message structures */

/* Message codes for uname operation */
#define MSG_UNAME		0x0A00	/* Get system information */
#define MSG_UNAME_REPLY		0x0A01	/* Reply with utsname data */

/* uname message structures */
struct msg_uname {
	struct mk_msg_header header;
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
	unsigned int flags;		/* Request flags */
};

struct msg_uname_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	char sysname[9];		/* Operating system name */
	char nodename[9];		/* Network node name */
	char release[9];		/* OS release */
	char version[9];		/* OS version */
	char machine[9];		/* Hardware type */
	unsigned int domain_id;		/* Capability domain ID */
};

/*
 * utsname structure -完全相同 ao original para compatibilidade
 * 
 * Original Linux 0.11: fixed-size 9-byte strings (8 chars + null)
 * Microkernel version: Same structure, but data comes from system server
 * and may be filtered based on caller's capabilities.
 */
struct utsname {
	char sysname[9];	/* Nome do sistema operacional */
	char nodename[9];	/* Nome do nó na rede */
	char release[9];	/* Release do sistema */
	char version[9];	/* Versão do sistema */
	char machine[9];	/* Tipo de hardware */
};

/* Flags for uname request */
#define UNAME_BASIC		0x00	/* Basic info (always available) */
#define UNAME_NODE		0x01	/* Request nodename (may need CAP) */
#define UNAME_ALL		0xFF	/* Request all info */

/**
 * uname - Get system information
 * @utsbuf: Pointer to utsname structure to fill
 *
 * Obtains system identification information from the system server.
 * The information may be filtered based on the caller's capabilities:
 * - All tasks can get sysname, release, machine
 * - Tasks with CAP_SYSTEM can also get nodename and version
 *
 * Returns 0 on success, -1 on error (with errno set).
 */
extern int uname(struct utsname *utsbuf);

/* Internal implementation as inline function */
static inline int __uname(struct utsname *utsbuf, unsigned int flags)
{
	struct msg_uname msg;
	struct msg_uname_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;
	
	if (!utsbuf) {
		return -1;  /* EFAULT - invalid pointer */
	}
	
	/* Prepare uname request message */
	msg.header.msg_id = MSG_UNAME;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	msg.flags = flags;
	
	/* Send to system server */
	result = mk_msg_send(kernel_state->system_server, &msg, sizeof(msg));
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
	
	/* Copy data to user buffer */
	memcpy(utsbuf->sysname, reply.sysname, 9);
	memcpy(utsbuf->nodename, reply.nodename, 9);
	memcpy(utsbuf->release, reply.release, 9);
	memcpy(utsbuf->version, reply.version, 9);
	memcpy(utsbuf->machine, reply.machine, 9);
	
	/* Ensure null termination (server should already do this) */
	utsbuf->sysname[8] = '\0';
	utsbuf->nodename[8] = '\0';
	utsbuf->release[8] = '\0';
	utsbuf->version[8] = '\0';
	utsbuf->machine[8] = '\0';
	
	return 0;
}

/* Public function implementation */
int uname(struct utsname *utsbuf)
{
	unsigned int flags = UNAME_BASIC;
	
	/* Tasks with CAP_SYSTEM can get full info */
	if (current_capability & CAP_SYSTEM) {
		flags = UNAME_ALL;
	}
	
	return __uname(utsbuf, flags);
}

/**
 * uname_domain - Get system information for specific capability domain
 * @utsbuf: Pointer to utsname structure
 * @domain_id: Capability domain ID
 *
 * Extended version for containers/virtualization. Tasks with
 * CAP_SYSTEM can get system info for other domains.
 */
static inline int uname_domain(struct utsname *utsbuf, unsigned int domain_id)
{
	/* Only tasks with CAP_SYSTEM can query other domains */
	if (!(current_capability & CAP_SYSTEM)) {
		return -1;  /* EPERM */
	}
	
	/* Would need extended message with domain_id field */
	/* For now, just return current domain info */
	return uname(utsbuf);
}

/**
 * gethostname - Get node name (POSIX compatibility)
 * @name: Buffer to store node name
 * @len: Buffer length
 *
 * Extracts nodename from uname() result.
 */
static inline int gethostname(char *name, size_t len)
{
	struct utsname uts;
	int result;
	
	if (!name || len == 0) {
		return -1;
	}
	
	result = uname(&uts);
	if (result < 0) {
		return result;
	}
	
	strncpy(name, uts.nodename, len - 1);
	name[len - 1] = '\0';
	
	return 0;
}

/**
 * sethostname - Set node name (requires CAP_SYSTEM)
 * @name: New node name
 * @len: Length of name
 *
 * Sets the system's node name. Only tasks with CAP_SYSTEM can do this.
 */
static inline int sethostname(const char *name, size_t len)
{
	struct msg_set_hostname msg;
	struct msg_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	/* Check capability */
	if (!(current_capability & CAP_SYSTEM)) {
		return -1;  /* EPERM */
	}
	
	if (!name || len == 0 || len >= 9) {
		return -1;  /* EINVAL */
	}
	
	/* Prepare message (would need new message type) */
	msg.header.msg_id = MSG_SET_HOSTNAME;  /* Would need definition */
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	memcpy(msg.newname, name, len);
	msg.newname[len] = '\0';
	
	if (mk_msg_send(kernel_state->system_server, &msg, sizeof(msg)) < 0) {
		return -1;
	}
	
	if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0) {
		return -1;
	}
	
	return reply.result;
}

/**
 * getdomainname - Get domain name (NIS/YP domain)
 * @name: Buffer to store domain name
 * @len: Buffer length
 */
static inline int getdomainname(char *name, size_t len)
{
	/* Domain name could be separate from nodename */
	/* Would need additional field in utsname or separate server call */
	strncpy(name, "local", len - 1);
	name[len - 1] = '\0';
	return 0;
}

/**
 * sysinfo - Get system information (extended)
 * @info: System information structure
 *
 * More detailed system information (load average, total memory, etc.)
 * Would be implemented by system server with appropriate capabilities.
 */
struct sysinfo {
	long uptime;			/* Seconds since boot */
	unsigned long loads[3];		/* 1, 5, and 15 minute load averages */
	unsigned long totalram;		/* Total usable main memory size */
	unsigned long freeram;		/* Available memory size */
	unsigned long sharedram;	/* Amount of shared memory */
	unsigned long bufferram;	/* Memory used by buffers */
	unsigned long totalswap;	/* Total swap space size */
	unsigned long freeswap;		/* Swap space still available */
	unsigned short procs;		/* Number of current processes */
	unsigned short domain;		/* Capability domain ID */
	char _f[22];			/* Padding */
};

static inline int sysinfo(struct sysinfo *info)
{
	struct msg_sysinfo msg;
	struct msg_sysinfo_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	/* Would need CAP_SYSTEM for detailed info? */
	
	msg.header.msg_id = MSG_SYSINFO;  /* Would need definition */
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	
	if (mk_msg_send(kernel_state->system_server, &msg, sizeof(msg)) < 0) {
		return -1;
	}
	
	if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0) {
		return -1;
	}
	
	if (reply.result < 0) {
		return reply.result;
	}
	
	memcpy(info, &reply.info, sizeof(struct sysinfo));
	return 0;
}

#endif /* _SYS_UTSNAME_H */
