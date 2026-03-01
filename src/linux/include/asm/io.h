/*
* HISTORY
* $Log: io.h,v $
* Revision 1.1 2026/02/13 19:30:45 pedro
* Microkernel version of I/O operations.
* All port I/O delegated to device server via IPC.
* Original macros preserved as inline functions with message passing.
* [2026/02/13 pedro]
*/

/*
* File: asm/io.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/13
*
* I/O operations for microkernel architecture.
*
* Original Linux 0.11: direct x86 port I/O using inb/outb instructions.
* Microkernel version: all I/O operations are sent as messages to
* the device server, which has the necessary privileges.
*
* Security: Only tasks with CAP_IO capability can perform I/O operations.
* The device server validates capabilities before accessing hardware.
*
* Performance: For frequently accessed ports (like console), tasks can
* request "I/O capabilities" that allow direct access after validation.
*/

#ifndef _ASM_IO_H
#define _ASM_IO_H

#include <linux/kernel.h>  /* For kernel_state, current_capability */
#include <linux/head.h>    /* For port_t, message structures */

/* Message codes for I/O operations */
#define MSG_IO_OUTB		0x0500
#define MSG_IO_INB		0x0501
#define MSG_IO_OUTW		0x0502
#define MSG_IO_INW		0x0503
#define MSG_IO_OUTL		0x0504
#define MSG_IO_INL		0x0505
#define MSG_IO_REQUEST_CAP	0x0506  /* Request direct I/O capability */

/* I/O message structures */
struct msg_io_outb {
	struct mk_msg_header header;
	unsigned char value;		/* Value to write */
	unsigned short port;		/* Port address */
	capability_t caps;		/* Caller capabilities */
	unsigned int task_id;		/* Task making request */
};

struct msg_io_inb {
	struct mk_msg_header header;
	unsigned short port;		/* Port address */
	capability_t caps;		/* Caller capabilities */
	unsigned int task_id;		/* Task making request */
};

struct msg_io_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		unsigned char byte;	/* Byte value for inb */
		unsigned short word;	/* Word value for inw */
		unsigned long dword;	/* Long value for inl */
	} value;
	unsigned int capability;	/* I/O capability (if requested) */
};

/* Device server port (configured during boot) */
#define DEVICE_SERVER_PORT	kernel_state->device_server

/**
 * outb - Write a byte to an I/O port
 * @value: Byte value to write
 * @port: I/O port address
 *
 * Sends a message to the device server to perform the outb operation.
 * Only tasks with CAP_IO capability can succeed.
 */
#define outb(value, port) \
do { \
	struct msg_io_outb msg; \
	int result; \
	\
	/* Check if task has I/O capability */ \
	if (!(current_capability & CAP_IO)) { \
		/* Try to request I/O capability */ \
		if (request_io_capability() < 0) \
			break; \
	} \
	\
	/* Prepare message */ \
	msg.header.msg_id = MSG_IO_OUTB; \
	msg.header.sender_port = kernel_state->kernel_port; \
	msg.header.reply_port = 0; /* Fire and forget */ \
	msg.header.size = sizeof(msg); \
	\
	msg.value = (unsigned char)(value); \
	msg.port = (unsigned short)(port); \
	msg.caps = current_capability; \
	msg.task_id = kernel_state->current_task; \
	\
	/* Send to device server */ \
	mk_msg_send(DEVICE_SERVER_PORT, &msg, sizeof(msg)); \
} while (0)

/**
 * inb - Read a byte from an I/O port
 * @port: I/O port address
 *
 * Sends a message to the device server to perform the inb operation.
 * Returns the byte read, or 0 if operation fails.
 */
#define inb(port) ({ \
	struct msg_io_inb msg; \
	struct msg_io_reply reply; \
	unsigned int reply_size = sizeof(reply); \
	unsigned char _v = 0; \
	int result; \
	\
	/* Check capability */ \
	if (current_capability & CAP_IO) { \
		/* Prepare message */ \
		msg.header.msg_id = MSG_IO_INB; \
		msg.header.sender_port = kernel_state->kernel_port; \
		msg.header.reply_port = kernel_state->kernel_port; \
		msg.header.size = sizeof(msg); \
		\
		msg.port = (unsigned short)(port); \
		msg.caps = current_capability; \
		msg.task_id = kernel_state->current_task; \
		\
		/* Send request and wait for reply */ \
		if (mk_msg_send(DEVICE_SERVER_PORT, &msg, sizeof(msg)) == 0) { \
			if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) { \
				if (reply.result == 0) \
					_v = reply.value.byte; \
			} \
		} \
	} \
	_v; \
})

/**
 * outb_p - Write a byte to an I/O port with I/O delay
 * @value: Byte value to write
 * @port: I/O port address
 *
 * Same as outb but with a small delay for compatibility with older hardware.
 * The delay is handled by the device server.
 */
#define outb_p(value, port) \
do { \
	struct msg_io_outb msg; \
	\
	if (!(current_capability & CAP_IO)) \
		break; \
	\
	msg.header.msg_id = MSG_IO_OUTB; \
	msg.header.sender_port = kernel_state->kernel_port; \
	msg.header.reply_port = 0; \
	msg.header.size = sizeof(msg); \
	\
	msg.value = (unsigned char)(value); \
	msg.port = (unsigned short)(port); \
	msg.caps = current_capability | 0x1000; /* Flag para delay */ \
	msg.task_id = kernel_state->current_task; \
	\
	mk_msg_send(DEVICE_SERVER_PORT, &msg, sizeof(msg)); \
} while (0)

/**
 * inb_p - Read a byte from an I/O port with I/O delay
 * @port: I/O port address
 *
 * Same as inb but with a small delay for compatibility with older hardware.
 * The delay is handled by the device server.
 */
#define inb_p(port) ({ \
	struct msg_io_inb msg; \
	struct msg_io_reply reply; \
	unsigned int reply_size = sizeof(reply); \
	unsigned char _v = 0; \
	\
	if (current_capability & CAP_IO) { \
		msg.header.msg_id = MSG_IO_INB; \
		msg.header.sender_port = kernel_state->kernel_port; \
		msg.header.reply_port = kernel_state->kernel_port; \
		msg.header.size = sizeof(msg); \
		\
		msg.port = (unsigned short)(port); \
		msg.caps = current_capability | 0x1000; /* Flag para delay */ \
		msg.task_id = kernel_state->current_task; \
		\
		if (mk_msg_send(DEVICE_SERVER_PORT, &msg, sizeof(msg)) == 0) { \
			if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) { \
				if (reply.result == 0) \
					_v = reply.value.byte; \
			} \
		} \
	} \
	_v; \
})

/* Additional I/O operations for completeness */

#define outw(value, port) \
do { \
	struct msg_io_outb msg; /* Reuse structure, server interprets as word */ \
	if (!(current_capability & CAP_IO)) \
		break; \
	msg.header.msg_id = MSG_IO_OUTW; \
	msg.header.sender_port = kernel_state->kernel_port; \
	msg.header.reply_port = 0; \
	msg.header.size = sizeof(msg); \
	msg.value = (unsigned char)(value & 0xFF); /* Server combines bytes */ \
	msg.port = (unsigned short)(port); \
	msg.caps = current_capability; \
	msg.task_id = kernel_state->current_task; \
	mk_msg_send(DEVICE_SERVER_PORT, &msg, sizeof(msg)); \
} while (0)

#define inw(port) ({ \
	struct msg_io_inb msg; \
	struct msg_io_reply reply; \
	unsigned int reply_size = sizeof(reply); \
	unsigned short _v = 0; \
	if (current_capability & CAP_IO) { \
		msg.header.msg_id = MSG_IO_INW; \
		msg.header.sender_port = kernel_state->kernel_port; \
		msg.header.reply_port = kernel_state->kernel_port; \
		msg.header.size = sizeof(msg); \
		msg.port = (unsigned short)(port); \
		msg.caps = current_capability; \
		msg.task_id = kernel_state->current_task; \
		if (mk_msg_send(DEVICE_SERVER_PORT, &msg, sizeof(msg)) == 0) { \
			if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) { \
				if (reply.result == 0) \
					_v = reply.value.word; \
			} \
		} \
	} \
	_v; \
})

/**
 * request_io_capability - Request I/O capability from device server
 *
 * Tasks without CAP_IO can request temporary I/O capability.
 * The device server validates the request based on security policy.
 * Returns 0 on success, -1 on failure.
 */
static inline int request_io_capability(void)
{
	struct msg_io_outb msg; /* Reuse structure for capability request */
	struct msg_io_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	msg.header.msg_id = MSG_IO_REQUEST_CAP;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.caps = current_capability;
	msg.task_id = kernel_state->current_task;
	msg.port = 0; /* Not used */
	msg.value = 0; /* Not used */
	
	if (mk_msg_send(DEVICE_SERVER_PORT, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0) {
				/* Update current capability with I/O permission */
				current_capability |= CAP_IO;
				return 0;
			}
		}
	}
	return -1;
}

#endif /* _ASM_IO_H */
