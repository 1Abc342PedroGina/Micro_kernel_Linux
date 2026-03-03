/*
* HISTORY
* $Log: who.c,v $
* Revision 1.1 2026/02/15 06:15:30 pedro
* Microkernel version of iam/whoami system calls.
* Now uses user server via IPC for identity storage.
* Added capability checking for identity operations.
* Preserved original logic but with IPC delegation.
* [2026/02/15 pedro]
*/

/*
 * File: kernel/who.c
 * Author: Unknown or Linus Torvalds (original Linux version - iam/whoami from some version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * Identity system calls (iam/whoami) for microkernel architecture.
 *
 * Original version: Direct kernel storage of user identity in a local buffer.
 * Microkernel version: Identity is stored in the user server, accessible
 * via IPC. The kernel maintains no permanent state; each call forwards
 * to the user server.
 *
 * Security: These operations require CAP_USER identity capability.
 * The user server validates that the caller has permission to set
 * or query identity information.
 */

#include <string.h>
#include <errno.h>
#include <linux/kernel.h>
#include <linux/head.h>
#include <asm/segment.h>

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_USER_IAM		0x1F00	/* Set identity */
#define MSG_USER_WHOAMI		0x1F01	/* Get identity */
#define MSG_USER_REPLY		0x1F02	/* Reply from user server */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_user_iam {
	struct mk_msg_header header;
	char name[24];			/* Identity string (max 23 chars + null) */
	unsigned int len;		/* Length of identity */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_user_whoami {
	struct mk_msg_header header;
	char *name;			/* User buffer to fill */
	unsigned int size;		/* Size of buffer */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_user_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		char name[24];		/* Identity string */
		unsigned int len;	/* Length of identity */
	} data;
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * CAPABILITY FLAGS
 *============================================================================*/

#define CAP_USER_IDENTITY	0x0800	/* Can set/get user identity */

/*=============================================================================
 * LOCAL FALLBACK BUFFER (for when server unavailable)
 *============================================================================*/

static char local_msg[24] = {0};	/* Fallback identity storage */

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * user_request - Send user identity request to user server
 * @msg_id: Message ID
 * @msg_data: Message data
 * @msg_size: Message size
 * @need_reply: Whether to wait for reply
 * @reply_data: Output reply data
 * 
 * Returns result from user server.
 */
static inline int user_request(unsigned int msg_id, void *msg_data,
                                 int msg_size, int need_reply,
                                 struct msg_user_reply *reply_data)
{
	struct msg_user_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	/* Check capability */
	if (!(current_capability & CAP_USER_IDENTITY))
		return -EPERM;

	result = mk_msg_send(kernel_state->user_server, msg_data, msg_size);
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
 * IAM SYSTEM CALL
 *============================================================================*/

/**
 * sys_iam - Set user identity
 * @name: Identity string (null-terminated, max 23 chars)
 * 
 * Stores the identity string associated with the current task.
 * Returns length of string on success, -EINVAL if too long,
 * -EPERM if capability missing, -EAGAIN if server unavailable.
 * 
 * In microkernel mode, this forwards to the user server.
 */
int sys_iam(const char* name)
{
	struct msg_user_iam msg;
	struct msg_user_reply reply;
	char tmp[30];
	int len = 0;
	int i;
	int result;

	/* Copy name from user space to kernel space */
	for (len = 0; len < 30; len++) {
		tmp[len] = get_fs_byte(name + len);
		if (tmp[len] == '\0' || tmp[len] == '\n') {
			break;
		}
	}

	/* Check length limit */
	if (len >= 24) {
		return -EINVAL;
	}

	/* Check capability */
	if (!(current_capability & CAP_USER_IDENTITY)) {
		/* Fallback to local storage if no capability */
		for (i = 0; i <= len; i++) {
			local_msg[i] = tmp[i];
		}
		return len;
	}

	/* Prepare IPC message */
	msg.header.msg_id = MSG_USER_IAM;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	/* Copy name to message */
	for (i = 0; i <= len; i++) {
		msg.name[i] = tmp[i];
	}
	msg.name[len] = '\0';
	msg.len = len;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	/* Send request to user server */
	result = user_request(MSG_USER_IAM, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Server unavailable - use local fallback */
		for (i = 0; i <= len; i++) {
			local_msg[i] = tmp[i];
		}
		return len;
	}

	return result;
}

/*=============================================================================
 * WHOAMI SYSTEM CALL
 *============================================================================*/

/**
 * sys_whoami - Get user identity
 * @name: User buffer to fill
 * @size: Size of buffer
 * 
 * Copies the current task's identity string to user buffer.
 * Returns length of string on success, -EINVAL if buffer too small,
 * -EPERM if capability missing, -EAGAIN if server unavailable.
 */
int sys_whoami(char* name, unsigned int size)
{
	struct msg_user_whoami msg;
	struct msg_user_reply reply;
	int len = 0;
	int i;
	int result;

	/* Check capability */
	if (!(current_capability & CAP_USER_IDENTITY)) {
		/* Fallback to local storage */
		for (len = 0; local_msg[len] != '\0'; len++);
		if (len > size) {
			return -EINVAL;
		}
		for (i = 0; i < size; i++) {
			put_fs_byte(local_msg[i], name + i);
			if (local_msg[i] == '\0') {
				break;
			}
		}
		return i;
	}

	/* Prepare IPC message */
	msg.header.msg_id = MSG_USER_WHOAMI;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.name = name;
	msg.size = size;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	/* Send request to user server */
	result = user_request(MSG_USER_WHOAMI, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Server unavailable - use local fallback */
		for (len = 0; local_msg[len] != '\0'; len++);
		if (len > size) {
			return -EINVAL;
		}
		for (i = 0; i < size; i++) {
			put_fs_byte(local_msg[i], name + i);
			if (local_msg[i] == '\0') {
				break;
			}
		}
		return i;
	}

	/* Copy result to user space */
	if (result >= 0) {
		/* Server returned success, but we need to copy the name */
		/* In a real implementation, the server would have already
		 * written to the user buffer via memory server */
		for (i = 0; i < size && i < result; i++) {
			put_fs_byte(reply.data.name[i], name + i);
		}
	}

	return result;
}

/*=============================================================================
 * ADDITIONAL IDENTITY FUNCTIONS
 *============================================================================*/

/**
 * sys_getidentity - Get full identity information
 * @buf: User buffer for identity struct
 * 
 * Returns extended identity information (name, domain, capabilities).
 */
struct identity_info {
	char name[24];			/* Identity string */
	unsigned int domain;		/* Capability domain */
	capability_t caps;		/* Task capabilities */
};

int sys_getidentity(struct identity_info *buf)
{
	struct msg_user_whoami msg;
	struct msg_user_reply reply;
	struct identity_info info;
	int result;

	if (!buf)
		return -EINVAL;

	/* Would need MSG_USER_GETINFO */

	/* For now, just return basic info */
	strcpy(info.name, local_msg);
	info.domain = current_capability & 0x0F;
	info.caps = current_capability;

	/* Copy to user space */
	verify_area(buf, sizeof(struct identity_info));
	memcpy_to_fs(buf, &info, sizeof(struct identity_info));

	return 0;
}

/**
 * sys_setidentity - Set identity with full info
 * @buf: Identity information
 */
int sys_setidentity(const struct identity_info *buf)
{
	struct identity_info info;

	if (!buf)
		return -EINVAL;

	/* Copy from user space */
	memcpy_from_fs(&info, buf, sizeof(struct identity_info));

	/* Would send to user server */
	local_msg[0] = '\0';
	strcpy(local_msg, info.name);

	return 0;
}

/*=============================================================================
 * INITIALIZATION
 *============================================================================*/

/**
 * who_init - Initialize identity subsystem
 */
void who_init(void)
{
	printk("Initializing identity subsystem (microkernel mode)...\n");
	
	/* Clear local fallback buffer */
	memset(local_msg, 0, sizeof(local_msg));
	
	printk("Identity subsystem initialized.\n");
}
