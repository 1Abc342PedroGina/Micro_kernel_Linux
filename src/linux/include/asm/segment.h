/*
* HISTORY
* $Log: segment.h,v $
* Revision 1.1 2026/02/13 21:30:15 pedro
* Microkernel version of segment operations.
* FS/GS segment selectors now represent capability spaces.
* All memory accesses via FS are delegated to memory server.
* Added capability validation for cross-space operations.
* [2026/02/13 pedro]
*/

/*
* File: asm/segment.h
* Author: Linus Torvalds (original Linux version)
*         Theodore Ts'o (original comments)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/13
*
* Segment operations for microkernel architecture.
*
* Original Linux 0.11: Direct x86 segment register access using FS for
* user data access (get_fs_byte, put_fs_byte, etc.).
*
* Microkernel version: FS/GS registers now contain capability space IDs
* rather than raw segment selectors. All memory accesses via FS are
* routed through the memory server, which validates capabilities and
* performs cross-space memory transfers.
*
* Security: Each task has multiple capability spaces:
* - DS: Kernel space (full access)
* - FS: User/Source space (read/write based on capabilities)
* - GS: Additional space (for IPC, shared memory)
*
* The memory server validates every cross-space access against the
* task's capabilities for the target space.
*/

#ifndef _ASM_SEGMENT_H
#define _ASM_SEGMENT_H

#include <linux/kernel.h>
#include <linux/head.h>

/* Message codes for segment operations */
#define MSG_SEG_GET_BYTE	0x0700
#define MSG_SEG_GET_WORD	0x0701
#define MSG_SEG_GET_LONG	0x0702
#define MSG_SEG_PUT_BYTE	0x0703
#define MSG_SEG_PUT_WORD	0x0704
#define MSG_SEG_PUT_LONG	0x0705
#define MSG_SEG_COPY_FROM	0x0706	/* Copy from FS to kernel */
#define MSG_SEG_COPY_TO		0x0707	/* Copy from kernel to FS */

/* Segment operation message structures */
struct msg_seg_get {
	struct mk_msg_header header;
	unsigned long addr;		/* Address in FS space */
	unsigned int space_id;		/* Capability space (FS/GS) */
	capability_t caps;		/* Caller capabilities */
	unsigned int task_id;		/* Task making request */
};

struct msg_seg_put {
	struct mk_msg_header header;
	unsigned long addr;		/* Address in FS space */
	unsigned long value;		/* Value to write */
	unsigned int space_id;		/* Capability space (FS/GS) */
	capability_t caps;		/* Caller capabilities */
	unsigned int task_id;		/* Task making request */
};

struct msg_seg_copy {
	struct mk_msg_header header;
	unsigned long from_addr;	/* Source address */
	unsigned long to_addr;		/* Destination address */
	unsigned long count;		/* Number of bytes */
	unsigned int from_space;	/* Source space (FS/GS) */
	unsigned int to_space;		/* Destination space (DS/FS/GS) */
	capability_t caps;		/* Caller capabilities */
	unsigned int task_id;		/* Task making request */
};

struct msg_seg_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		unsigned char byte;
		unsigned short word;
		unsigned long dword;
		unsigned long value;	/* For general use */
	} data;
};

/* Capability space identifiers */
#define SPACE_KERNEL	0	/* Kernel space (DS) */
#define SPACE_USER	1	/* User space (FS) - current task */
#define SPACE_ALT	2	/* Alternative space (GS) */
#define SPACE_IPC	3	/* IPC temporary space */

/* Current space IDs (stored in segment registers) */
extern unsigned int current_fs_space;	/* Current FS capability space */
extern unsigned int current_gs_space;	/* Current GS capability space */

/**
 * get_fs_byte - Read a byte from FS address space
 * @addr: Address in FS space
 *
 * Reads a single byte from the address space identified by the FS register.
 * The memory server validates that the current task has read capability
 * for the specified address in the FS space.
 *
 * Returns the byte read, or 0 if access is denied.
 */
static inline unsigned char get_fs_byte(const char * addr)
{
	struct msg_seg_get msg;
	struct msg_seg_reply reply;
	unsigned int reply_size = sizeof(reply);
	unsigned char _v = 0;
	
	/* Check if we have memory capability */
	if (!(current_capability & CAP_MEMORY)) {
		if (request_memory_capability() < 0)
			return 0;
	}
	
	/* Prepare message */
	msg.header.msg_id = MSG_SEG_GET_BYTE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.addr = (unsigned long)addr;
	msg.space_id = current_fs_space;  /* Current FS capability space */
	msg.caps = current_capability;
	msg.task_id = kernel_state->current_task;
	
	/* Send to memory server and wait for reply */
	if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0)
				_v = reply.data.byte;
		}
	}
	
	return _v;
}

/**
 * get_fs_word - Read a word (16 bits) from FS address space
 * @addr: Address in FS space (must be word-aligned)
 */
static inline unsigned short get_fs_word(const unsigned short *addr)
{
	struct msg_seg_get msg;
	struct msg_seg_reply reply;
	unsigned int reply_size = sizeof(reply);
	unsigned short _v = 0;
	
	if (!(current_capability & CAP_MEMORY))
		return 0;
	
	msg.header.msg_id = MSG_SEG_GET_WORD;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.addr = (unsigned long)addr;
	msg.space_id = current_fs_space;
	msg.caps = current_capability;
	msg.task_id = kernel_state->current_task;
	
	if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0)
				_v = reply.data.word;
		}
	}
	
	return _v;
}

/**
 * get_fs_long - Read a long (32 bits) from FS address space
 * @addr: Address in FS space (must be long-aligned)
 */
static inline unsigned long get_fs_long(const unsigned long *addr)
{
	struct msg_seg_get msg;
	struct msg_seg_reply reply;
	unsigned int reply_size = sizeof(reply);
	unsigned long _v = 0;
	
	if (!(current_capability & CAP_MEMORY))
		return 0;
	
	msg.header.msg_id = MSG_SEG_GET_LONG;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.addr = (unsigned long)addr;
	msg.space_id = current_fs_space;
	msg.caps = current_capability;
	msg.task_id = kernel_state->current_task;
	
	if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0)
				_v = reply.data.dword;
		}
	}
	
	return _v;
}

/**
 * put_fs_byte - Write a byte to FS address space
 * @val: Byte value to write
 * @addr: Address in FS space
 *
 * Writes a single byte to the FS address space. The memory server
 * validates that the current task has write capability for the
 * specified address.
 */
static inline void put_fs_byte(char val, char *addr)
{
	struct msg_seg_put msg;
	
	if (!(current_capability & CAP_MEMORY))
		return;
	
	msg.header.msg_id = MSG_SEG_PUT_BYTE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;  /* Fire and forget */
	msg.header.size = sizeof(msg);
	
	msg.addr = (unsigned long)addr;
	msg.value = (unsigned long)(unsigned char)val;
	msg.space_id = current_fs_space;
	msg.caps = current_capability;
	msg.task_id = kernel_state->current_task;
	
	mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg));
}

/**
 * put_fs_word - Write a word to FS address space
 * @val: Word value to write
 * @addr: Address in FS space
 */
static inline void put_fs_word(short val, short *addr)
{
	struct msg_seg_put msg;
	
	if (!(current_capability & CAP_MEMORY))
		return;
	
	msg.header.msg_id = MSG_SEG_PUT_WORD;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.addr = (unsigned long)addr;
	msg.value = (unsigned long)(unsigned short)val;
	msg.space_id = current_fs_space;
	msg.caps = current_capability;
	msg.task_id = kernel_state->current_task;
	
	mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg));
}

/**
 * put_fs_long - Write a long to FS address space
 * @val: Long value to write
 * @addr: Address in FS space
 */
static inline void put_fs_long(unsigned long val, unsigned long *addr)
{
	struct msg_seg_put msg;
	
	if (!(current_capability & CAP_MEMORY))
		return;
	
	msg.header.msg_id = MSG_SEG_PUT_LONG;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.addr = (unsigned long)addr;
	msg.value = val;
	msg.space_id = current_fs_space;
	msg.caps = current_capability;
	msg.task_id = kernel_state->current_task;
	
	mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg));
}

/**
 * copy_from_fs - Copy block from FS space to kernel space
 * @to: Kernel destination address
 * @from: FS source address
 * @n: Number of bytes to copy
 *
 * Bulk copy operation from user space (FS) to kernel space.
 * More efficient than multiple get_fs_byte calls.
 */
static inline unsigned long copy_from_fs(void *to, const void *from, unsigned long n)
{
	struct msg_seg_copy msg;
	struct msg_seg_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	if (!(current_capability & CAP_MEMORY))
		return n;  /* Return number of bytes not copied */
	
	msg.header.msg_id = MSG_SEG_COPY_FROM;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.from_addr = (unsigned long)from;
	msg.to_addr = (unsigned long)to;
	msg.count = n;
	msg.from_space = current_fs_space;
	msg.to_space = SPACE_KERNEL;  /* Kernel space */
	msg.caps = current_capability;
	msg.task_id = kernel_state->current_task;
	
	if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result >= 0)
				return reply.result;  /* Bytes actually copied */
		}
	}
	
	return n;  /* Assume nothing was copied */
}

/**
 * copy_to_fs - Copy block from kernel space to FS space
 * @to: FS destination address
 * @from: Kernel source address
 * @n: Number of bytes to copy
 */
static inline unsigned long copy_to_fs(void *to, const void *from, unsigned long n)
{
	struct msg_seg_copy msg;
	struct msg_seg_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	if (!(current_capability & CAP_MEMORY))
		return n;
	
	msg.header.msg_id = MSG_SEG_COPY_TO;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.from_addr = (unsigned long)from;
	msg.to_addr = (unsigned long)to;
	msg.count = n;
	msg.from_space = SPACE_KERNEL;
	msg.to_space = current_fs_space;
	msg.caps = current_capability;
	msg.task_id = kernel_state->current_task;
	
	if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result >= 0)
				return reply.result;
		}
	}
	
	return n;
}

/**
 * get_fs - Get current FS capability space ID
 *
 * Returns the ID of the current FS capability space.
 * This is stored in the FS segment register but interpreted
 * as a capability space index rather than a segment selector.
 */
static inline unsigned long get_fs(void)
{
	return current_fs_space;
}

/**
 * get_ds - Get kernel capability space ID
 *
 * Returns the ID of the kernel capability space (always SPACE_KERNEL).
 * In microkernel mode, DS always refers to the kernel's address space.
 */
static inline unsigned long get_ds(void)
{
	return SPACE_KERNEL;
}

/**
 * set_fs - Set current FS capability space
 * @val: New capability space ID
 *
 * Changes the current FS capability space. This affects all subsequent
 * get_fs_* and put_fs_* operations. The kernel validates that the task
 * has access to the specified capability space.
 */
static inline void set_fs(unsigned long val)
{
	struct msg_seg_set_space msg;  /* Would need to define this */
	
	/* Validate that we can switch to this space */
	if (val < MAX_CAP_SPACES && task_has_space(kernel_state->current_task, val)) {
		current_fs_space = (unsigned int)val;
		
		/* Notify memory server of space change (optional) */
		msg.header.msg_id = MSG_SEG_SET_FS;
		msg.header.sender_port = kernel_state->kernel_port;
		msg.header.reply_port = 0;
		msg.header.size = sizeof(msg);
		msg.space_id = val;
		msg.task_id = kernel_state->current_task;
		
		mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg));
	}
}

/**
 * set_gs - Set current GS capability space
 * @val: New capability space ID
 *
 * Similar to set_fs but for the GS register (used for alternative spaces).
 */
static inline void set_gs(unsigned long val)
{
	if (val < MAX_CAP_SPACES && task_has_space(kernel_state->current_task, val)) {
		current_gs_space = (unsigned int)val;
	}
}

/**
 * task_has_space - Check if task has access to capability space
 * @task: Task ID
 * @space: Capability space ID
 *
 * Internal function to validate space access.
 */
static inline int task_has_space(unsigned int task, unsigned int space)
{
	/* In a real implementation, consult capability table */
	/* For now, assume kernel has all spaces, user has only SPACE_USER */
	if (task == 0)  /* Kernel task */
		return 1;
	return (space == SPACE_USER);
}

/* Compatibility macros */
#define get_user_byte(addr)	get_fs_byte(addr)
#define put_user_byte(val,addr) put_fs_byte(val,addr)
#define get_user_word(addr)	get_fs_word(addr)
#define put_user_word(val,addr) put_fs_word(val,addr)
#define get_user_long(addr)	get_fs_long(addr)
#define put_user_long(val,addr) put_fs_long(val,addr)

#endif /* _ASM_SEGMENT_H */
