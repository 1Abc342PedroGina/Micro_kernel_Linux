/*
* HISTORY
* $Log: memory.h,v $
* Revision 1.1 2026/02/13 20:15:30 pedro
* Microkernel version of memory operations.
* memcpy, memmove, memset, memcmp now use memory server via IPC.
* Added capability checking for all operations.
* [2026/02/13 pedro]
*/

/*
* File: asm/memory.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/13
*
* Memory operations for microkernel architecture.
*
* Original Linux 0.11: direct assembly memcpy using movsb instruction.
* Microkernel version: all memory operations are sent as messages to
* the memory server, which validates capabilities and performs the
* actual memory access.
*
* Security: All operations check CAP_MEMORY capability. The memory server
* verifies that the source and destination regions are accessible to the
* calling task before performing any operation.
*
* NOTE: In microkernel mode, ds, es, fs, gs selectors are still present
* but now represent capability spaces rather than raw segment selectors.
*/

#ifndef _ASM_MEMORY_H
#define _ASM_MEMORY_H

#include <linux/kernel.h>  /* For kernel_state, current_capability */
#include <linux/head.h>    /* For port_t, message structures */

/* Message codes for memory operations */
#define MSG_MEM_COPY		0x0600	/* Copy memory region */
#define MSG_MEM_SET		0x0601	/* Set memory region to value */
#define MSG_MEM_CMP		0x0602	/* Compare memory regions */
#define MSG_MEM_MOVE		0x0603	/* Move memory region (handles overlap) */
#define MSG_MEM_ZERO		0x0604	/* Zero memory region */

/* Memory operation message structures */
struct msg_mem_copy {
	struct mk_msg_header header;
	unsigned long dest;		/* Destination address (virtual) */
	unsigned long src;		/* Source address (virtual) */
	unsigned long n;		/* Number of bytes */
	capability_t caps;		/* Caller capabilities */
	unsigned int task_id;		/* Task making request */
	unsigned int dest_space;	/* Destination address space */
	unsigned int src_space;		/* Source address space */
};

struct msg_mem_set {
	struct mk_msg_header header;
	unsigned long dest;		/* Destination address */
	unsigned char value;		/* Value to set */
	unsigned long n;		/* Number of bytes */
	capability_t caps;		/* Caller capabilities */
	unsigned int task_id;		/* Task making request */
	unsigned int space_id;		/* Address space */
};

struct msg_mem_cmp {
	struct mk_msg_header header;
	unsigned long s1;		/* First region */
	unsigned long s2;		/* Second region */
	unsigned long n;		/* Number of bytes */
	capability_t caps;		/* Caller capabilities */
	unsigned int task_id;		/* Task making request */
	unsigned int space1_id;		/* First address space */
	unsigned int space2_id;		/* Second address space */
};

struct msg_mem_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		unsigned long value;	/* Return value (for memcmp) */
		struct {
			unsigned long dest;	/* Destination for confirmation */
			unsigned long src;	/* Source for confirmation */
		} addr;
	} data;
};

/**
 * memcpy - Copy memory region
 * @dest: Destination address
 * @src: Source address
 * @n: Number of bytes to copy
 *
 * Sends a message to the memory server to perform the copy.
 * The server validates that both regions are accessible to the task
 * with appropriate capabilities (read from src, write to dest).
 *
 * Returns pointer to destination.
 *
 * NOTE: In microkernel mode, the caller must have CAP_MEMORY capability
 * and appropriate access rights to both memory regions.
 */
#define memcpy(dest, src, n) ({ \
	void *_res = (void *)(dest); \
	struct msg_mem_copy msg; \
	struct msg_mem_reply reply; \
	unsigned int reply_size = sizeof(reply); \
	\
	/* Check capability */ \
	if (current_capability & CAP_MEMORY) { \
		/* Prepare message */ \
		msg.header.msg_id = MSG_MEM_COPY; \
		msg.header.sender_port = kernel_state->kernel_port; \
		msg.header.reply_port = kernel_state->kernel_port; \
		msg.header.size = sizeof(msg); \
		\
		msg.dest = (unsigned long)(dest); \
		msg.src = (unsigned long)(src); \
		msg.n = (unsigned long)(n); \
		msg.caps = current_capability; \
		msg.task_id = kernel_state->current_task; \
		msg.dest_space = kernel_state->current_space; /* Current address space */ \
		msg.src_space = kernel_state->current_space; \
		\
		/* Send to memory server and wait for confirmation */ \
		if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) == 0) { \
			if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) { \
				if (reply.result == 0) { \
					/* Operation successful */ \
				} else { \
					_res = NULL; /* Error indicator */ \
				} \
			} else { \
				_res = NULL; \
			} \
		} else { \
			_res = NULL; \
		} \
	} else { \
		/* Try to request memory capability */ \
		if (request_memory_capability() < 0) \
			_res = NULL; \
		else \
			_res = memcpy(dest, src, n); /* Retry with capability */ \
	} \
	_res; \
})

/**
 * memmove - Copy memory region (handles overlap)
 * @dest: Destination address
 * @src: Source address
 * @n: Number of bytes
 *
 * Similar to memcpy but handles overlapping regions correctly.
 * The memory server determines the direction based on address comparison.
 */
#define memmove(dest, src, n) ({ \
	void *_res = (void *)(dest); \
	struct msg_mem_copy msg; \
	struct msg_mem_reply reply; \
	unsigned int reply_size = sizeof(reply); \
	\
	if (current_capability & CAP_MEMORY) { \
		msg.header.msg_id = MSG_MEM_MOVE; /* Different opcode */ \
		msg.header.sender_port = kernel_state->kernel_port; \
		msg.header.reply_port = kernel_state->kernel_port; \
		msg.header.size = sizeof(msg); \
		\
		msg.dest = (unsigned long)(dest); \
		msg.src = (unsigned long)(src); \
		msg.n = (unsigned long)(n); \
		msg.caps = current_capability; \
		msg.task_id = kernel_state->current_task; \
		msg.dest_space = kernel_state->current_space; \
		msg.src_space = kernel_state->current_space; \
		\
		if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) == 0) { \
			if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) { \
				if (reply.result != 0) \
					_res = NULL; \
			} else { \
				_res = NULL; \
			} \
		} else { \
			_res = NULL; \
		} \
	} else { \
		_res = NULL; \
	} \
	_res; \
})

/**
 * memset - Fill memory with constant byte
 * @s: Memory region start
 * @c: Byte value
 * @n: Number of bytes
 *
 * Sets n bytes of memory starting at s to the byte value c.
 * Server validates write access to the region.
 */
#define memset(s, c, n) ({ \
	void *_res = (void *)(s); \
	struct msg_mem_set msg; \
	struct msg_mem_reply reply; \
	unsigned int reply_size = sizeof(reply); \
	\
	if (current_capability & CAP_MEMORY) { \
		msg.header.msg_id = MSG_MEM_SET; \
		msg.header.sender_port = kernel_state->kernel_port; \
		msg.header.reply_port = kernel_state->kernel_port; \
		msg.header.size = sizeof(msg); \
		\
		msg.dest = (unsigned long)(s); \
		msg.value = (unsigned char)(c); \
		msg.n = (unsigned long)(n); \
		msg.caps = current_capability; \
		msg.task_id = kernel_state->current_task; \
		msg.space_id = kernel_state->current_space; \
		\
		if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) == 0) { \
			if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) { \
				if (reply.result != 0) \
					_res = NULL; \
			} else { \
				_res = NULL; \
			} \
		} else { \
			_res = NULL; \
		} \
	} else { \
		_res = NULL; \
	} \
	_res; \
})

/**
 * memcmp - Compare two memory regions
 * @s1: First region
 * @s2: Second region
 * @n: Number of bytes to compare
 *
 * Compares the first n bytes of s1 and s2.
 * Returns 0 if equal, <0 if s1 < s2, >0 if s1 > s2.
 */
#define memcmp(s1, s2, n) ({ \
	int _res = 0; \
	struct msg_mem_cmp msg; \
	struct msg_mem_reply reply; \
	unsigned int reply_size = sizeof(reply); \
	\
	if (current_capability & CAP_MEMORY) { \
		msg.header.msg_id = MSG_MEM_CMP; \
		msg.header.sender_port = kernel_state->kernel_port; \
		msg.header.reply_port = kernel_state->kernel_port; \
		msg.header.size = sizeof(msg); \
		\
		msg.s1 = (unsigned long)(s1); \
		msg.s2 = (unsigned long)(s2); \
		msg.n = (unsigned long)(n); \
		msg.caps = current_capability; \
		msg.task_id = kernel_state->current_task; \
		msg.space1_id = kernel_state->current_space; \
		msg.space2_id = kernel_state->current_space; \
		\
		if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) == 0) { \
			if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) { \
				_res = reply.data.value; /* Comparison result */ \
			} \
		} \
	} \
	_res; \
})

/**
 * memzero - Zero memory region
 * @s: Memory region start
 * @n: Number of bytes
 *
 * Optimized version of memset(s, 0, n)
 */
#define memzero(s, n) ({ \
	struct msg_mem_set msg; \
	struct msg_mem_reply reply; \
	unsigned int reply_size = sizeof(reply); \
	int _res = 0; \
	\
	if (current_capability & CAP_MEMORY) { \
		msg.header.msg_id = MSG_MEM_ZERO; \
		msg.header.sender_port = kernel_state->kernel_port; \
		msg.header.reply_port = kernel_state->kernel_port; \
		msg.header.size = sizeof(msg); \
		\
		msg.dest = (unsigned long)(s); \
		msg.value = 0; \
		msg.n = (unsigned long)(n); \
		msg.caps = current_capability; \
		msg.task_id = kernel_state->current_task; \
		msg.space_id = kernel_state->current_space; \
		\
		if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) == 0) { \
			if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) { \
				_res = reply.result; \
			} \
		} \
	} \
	_res; \
})

/**
 * request_memory_capability - Request memory capability
 *
 * Tasks without CAP_MEMORY can request temporary memory access.
 * Returns 0 on success, -1 on failure.
 */
static inline int request_memory_capability(void)
{
	struct msg_mem_copy msg; /* Reuse structure for capability request */
	struct msg_mem_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	msg.header.msg_id = MSG_MEM_REQUEST_CAP; /* Would need to define this */
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.caps = current_capability;
	msg.task_id = kernel_state->current_task;
	/* Other fields zero */
	
	if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0) {
				current_capability |= CAP_MEMORY;
				return 0;
			}
		}
	}
	return -1;
}

/* Compatibility macros for code that expects the original behavior */
#define __memcpy(dest, src, n) memcpy(dest, src, n)
#define __memset(s, c, n) memset(s, c, n)
#define __memcmp(s1, s2, n) memcmp(s1, s2, n)

#endif /* _ASM_MEMORY_H */
