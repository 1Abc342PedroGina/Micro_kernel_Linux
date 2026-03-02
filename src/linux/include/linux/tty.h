/*
* HISTORY
* $Log: tty.h,v $
* Revision 1.1 2026/02/14 16:30:45 pedro
* Microkernel version of terminal I/O definitions.
* Original structures preserved for compatibility.
* tty_queue now represents IPC message queues.
* All tty operations send messages to terminal server.
* [2026/02/14 pedro]
*/

/*
* File: linux/tty.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* Terminal I/O definitions for microkernel architecture.
*
* Original Linux 0.11: Kernel-level terminal structures with direct
* hardware access (serial, console).
*
* Microkernel version: All structures are now templates for communication
* with the terminal server. The actual tty queues, termios settings, and
* hardware access are managed by the terminal server.
*
* NOTE! Don't touch this without checking that nothing in rs_io.s or
* con_io.s breaks. Some constants are hardwired into the system (mainly
* offsets into 'tty_queue'). In microkernel mode, these offsets now
* refer to positions in IPC message queues.
*/

#ifndef _TTY_H
#define _TTY_H

#include <termios.h>
#include <linux/kernel.h>
#include <linux/head.h>

/*=============================================================================
 * ORIGINAL CONSTANTS (Preserved for compatibility)
 *============================================================================*/

#define TTY_BUF_SIZE 1024

/*=============================================================================
 * ORIGINAL QUEUE STRUCTURE (Preserved as template)
 *============================================================================*/

/*
 * tty_queue - Terminal queue structure
 * 
 * Original: Kernel buffer queues for terminal I/O.
 * Microkernel: Now represents a queue of IPC messages in the terminal server.
 * The fields have new meanings:
 * - data: Queue state/capability flags
 * - head: Next write position (managed by server)
 * - tail: Next read position (managed by server)
 * - proc_list: List of tasks waiting on this queue
 * - buf: Buffer for data (in server's memory space)
 */
struct tty_queue {
	unsigned long data;		/* Queue state / capability flags */
	unsigned long head;		/* Next write position */
	unsigned long tail;		/* Next read position */
	struct task_struct * proc_list;	/* Waiting tasks */
	char buf[TTY_BUF_SIZE];	/* Data buffer */
};

/*=============================================================================
 * ORIGINAL QUEUE MACROS (Now operate on server-managed queues)
 *============================================================================*/

#define INC(a) ((a) = ((a)+1) & (TTY_BUF_SIZE-1))
#define DEC(a) ((a) = ((a)-1) & (TTY_BUF_SIZE-1))
#define EMPTY(a) ((a).head == (a).tail)
#define LEFT(a) (((a).tail-(a).head-1)&(TTY_BUF_SIZE-1))
#define LAST(a) ((a).buf[(TTY_BUF_SIZE-1)&((a).head-1)])
#define FULL(a) (!LEFT(a))
#define CHARS(a) (((a).head-(a).tail)&(TTY_BUF_SIZE-1))
#define GETCH(queue,c) \
(void)({c=(queue).buf[(queue).tail];INC((queue).tail);})
#define PUTCH(c,queue) \
(void)({(queue).buf[(queue).head]=(c);INC((queue).head);})

/*=============================================================================
 * ORIGINAL TERMIOS ACCESS MACROS (Preserved)
 *============================================================================*/

#define INTR_CHAR(tty) ((tty)->termios.c_cc[VINTR])
#define QUIT_CHAR(tty) ((tty)->termios.c_cc[VQUIT])
#define ERASE_CHAR(tty) ((tty)->termios.c_cc[VERASE])
#define KILL_CHAR(tty) ((tty)->termios.c_cc[VKILL])
#define EOF_CHAR(tty) ((tty)->termios.c_cc[VEOF])
#define START_CHAR(tty) ((tty)->termios.c_cc[VSTART])
#define STOP_CHAR(tty) ((tty)->termios.c_cc[VSTOP])
#define SUSPEND_CHAR(tty) ((tty)->termios.c_cc[VSUSP])

/*=============================================================================
 * ORIGINAL TTY STRUCTURE (Preserved as template)
 *============================================================================*/

/*
 * tty_struct - Terminal structure
 * 
 * Original: Kernel structure for each terminal device.
 * Microkernel: Represents a virtual terminal in the terminal server.
 * Each tty_struct is identified by a capability and accessed via IPC.
 */
struct tty_struct {
	struct termios termios;		/* Terminal settings */
	int pgrp;			/* Process group */
	int stopped;			/* Stopped flag */
	void (*write)(struct tty_struct * tty);	/* Write function (server-side) */
	struct tty_queue read_q;	/* Read queue */
	struct tty_queue write_q;	/* Write queue */
	struct tty_queue secondary;	/* Secondary queue (cooked mode) */
};

/*=============================================================================
 * ORIGINAL EXTERNAL VARIABLES (Now managed by terminal server)
 *============================================================================*/

extern struct tty_struct tty_table[];

/*=============================================================================
 * ORIGINAL INIT STRING (Preserved)
 *============================================================================*/

/*	intr=^C		quit=^|		erase=del	kill=^U
	eof=^D		vtime=\0	vmin=\1		sxtc=\0
	start=^Q	stop=^S		susp=^Z		eol=\0
	reprint=^R	discard=^U	werase=^W	lnext=^V
	eol2=\0
*/
#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0"

/*=============================================================================
 * ORIGINAL FUNCTION PROTOTYPES (Now stubs that send IPC)
 *============================================================================*/

void rs_init(void);
void con_init(void);
void tty_init(void);

int tty_read(unsigned c, char * buf, int n);
int tty_write(unsigned c, char * buf, int n);

void rs_write(struct tty_struct * tty);
void con_write(struct tty_struct * tty);

void copy_to_cooked(struct tty_struct * tty);

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_TTY_RS_INIT	0x1300	/* Initialize serial */
#define MSG_TTY_CON_INIT	0x1301	/* Initialize console */
#define MSG_TTY_INIT		0x1302	/* Initialize all ttys */
#define MSG_TTY_READ		0x1303	/* Read from tty */
#define MSG_TTY_WRITE		0x1304	/* Write to tty */
#define MSG_TTY_RS_WRITE	0x1305	/* Serial write */
#define MSG_TTY_CON_WRITE	0x1306	/* Console write */
#define MSG_TTY_COOKED		0x1307	/* Copy to cooked mode */
#define MSG_TTY_REPLY		0x1308	/* Reply from tty server */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_tty_rw {
	struct mk_msg_header header;
	unsigned int channel;		/* Tty channel number */
	char *buf;			/* User buffer */
	int count;			/* Number of bytes */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_tty_write_func {
	struct mk_msg_header header;
	struct tty_struct *tty;	/* Tty structure */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_tty_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		int bytes;		/* Bytes transferred */
		struct termios termios;	/* Terminal settings */
	} data;
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * CAPABILITY FLAGS FOR TTY OPERATIONS
 *============================================================================*/

#define CAP_TTY_READ		0x0010	/* Can read from tty */
#define CAP_TTY_WRITE		0x0020	/* Can write to tty */
#define CAP_TTY_CONF		0x0040	/* Can configure tty */
#define CAP_TTY_ADMIN		0x0080	/* Administrative operations */

/*=============================================================================
 * TTY NUMBER DEFINITIONS
 *============================================================================*/

#define TTY_CONSOLE	0	/* Console tty */
#define TTY_SERIAL0	1	/* First serial port */
#define TTY_SERIAL1	2	/* Second serial port */
#define TTY_VIRTUAL0	3	/* First virtual terminal */
#define MAX_TTYS	8	/* Maximum number of ttys */

/*=============================================================================
 * FUNCTION IMPLEMENTATIONS (Stubs that send IPC)
 *============================================================================*/

/**
 * __tty_request - Send tty request to terminal server
 * @msg_id: Message ID
 * @channel: Tty channel
 * @buf: Buffer
 * @count: Count
 * @need_reply: Whether to wait for reply
 * 
 * Returns result from terminal server.
 */
static inline int __tty_request(unsigned int msg_id, unsigned int channel,
                                 char *buf, int count, int need_reply)
{
	struct msg_tty_rw msg;
	struct msg_tty_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	/* Check appropriate capability */
	if ((msg_id == MSG_TTY_READ) && !(current_capability & CAP_TTY_READ))
		return -1;
	if ((msg_id == MSG_TTY_WRITE) && !(current_capability & CAP_TTY_WRITE))
		return -1;

	msg.header.msg_id = msg_id;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = need_reply ? kernel_state->kernel_port : 0;
	msg.header.size = sizeof(msg);

	msg.channel = channel;
	msg.buf = buf;
	msg.count = count;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	if (!need_reply)
		return 0;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	/* Update capabilities if granted */
	if (reply.granted_caps)
		current_capability |= reply.granted_caps;

	return (reply.result < 0) ? reply.result : reply.data.bytes;
}

/**
 * rs_init - Initialize serial ports
 * 
 * Sends request to terminal server to initialize serial hardware.
 */
void rs_init(void)
{
	struct msg_tty_rw msg;
	
	if (!(current_capability & CAP_TTY_ADMIN))
		return;

	msg.header.msg_id = MSG_TTY_RS_INIT;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg));
}

/**
 * con_init - Initialize console
 * 
 * Sends request to terminal server to initialize console.
 */
void con_init(void)
{
	struct msg_tty_rw msg;
	
	if (!(current_capability & CAP_TTY_ADMIN))
		return;

	msg.header.msg_id = MSG_TTY_CON_INIT;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg));
}

/**
 * tty_init - Initialize all ttys
 * 
 * Sends request to terminal server to initialize all terminals.
 */
void tty_init(void)
{
	struct msg_tty_rw msg;
	
	if (!(current_capability & CAP_TTY_ADMIN))
		return;

	msg.header.msg_id = MSG_TTY_INIT;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg));
}

/**
 * tty_read - Read from terminal
 * @c: Channel number
 * @buf: User buffer
 * @n: Number of bytes to read
 * 
 * Returns number of bytes read, or -1 on error.
 */
int tty_read(unsigned c, char * buf, int n)
{
	if (c >= MAX_TTYS)
		return -1;
	
	return __tty_request(MSG_TTY_READ, c, buf, n, 1);
}

/**
 * tty_write - Write to terminal
 * @c: Channel number
 * @buf: User buffer
 * @n: Number of bytes to write
 * 
 * Returns number of bytes written, or -1 on error.
 */
int tty_write(unsigned c, char * buf, int n)
{
	if (c >= MAX_TTYS)
		return -1;
	
	return __tty_request(MSG_TTY_WRITE, c, buf, n, 1);
}

/**
 * rs_write - Serial write function
 * @tty: Tty structure
 * 
 * Called by terminal server to write to serial port.
 * This would be implemented in the server, not in client stubs.
 */
void rs_write(struct tty_struct * tty)
{
	struct msg_tty_write_func msg;
	
	/* This function runs in the terminal server, not in clients */
	/* The stub is provided for completeness */
	
	msg.header.msg_id = MSG_TTY_RS_WRITE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.tty = tty;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg));
}

/**
 * con_write - Console write function
 * @tty: Tty structure
 * 
 * Called by terminal server to write to console.
 */
void con_write(struct tty_struct * tty)
{
	struct msg_tty_write_func msg;
	
	msg.header.msg_id = MSG_TTY_CON_WRITE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.tty = tty;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg));
}

/**
 * copy_to_cooked - Copy raw input to cooked mode queue
 * @tty: Tty structure
 * 
 * Process raw input according to termios settings.
 * Implemented in terminal server.
 */
void copy_to_cooked(struct tty_struct * tty)
{
	struct msg_tty_write_func msg;
	
	msg.header.msg_id = MSG_TTY_COOKED;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.tty = tty;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg));
}

/*=============================================================================
 * TERMINAL SERVER STATE (for reference)
 *============================================================================*/

/*
 * This structure would be maintained by the terminal server.
 * Not part of the client API.
 */
struct tty_server_state {
	struct tty_struct ttys[MAX_TTYS];	/* Terminal structures */
	unsigned int next_virtual_tty;		/* Next free VT number */
	capability_t tty_caps[MAX_TTYS];	/* Capabilities per tty */
	
	/* Hardware state (managed by server) */
	struct {
		int port_base;			/* Serial port base address */
		int irq;			/* IRQ number */
		int speed;			/* Baud rate */
	} serial[2];
	
	struct {
		int video_memory;		/* Video memory address */
		int cursor_x, cursor_y;	/* Cursor position */
		int attrib;			/* Current attribute */
	} console;
};

/*=============================================================================
 * CAPABILITY REQUEST FUNCTION
 *============================================================================*/

/**
 * request_tty_capability - Request tty capability
 * @cap: Capability to request (CAP_TTY_*)
 * 
 * Returns 0 on success, -1 on error.
 */
static inline int request_tty_capability(unsigned int cap)
{
	struct msg_cap_request msg;
	struct msg_cap_reply reply;
	unsigned int reply_size = sizeof(reply);

	msg.header.msg_id = MSG_CAP_REQUEST_TTY;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.task_id = kernel_state->current_task;
	msg.requested_cap = cap;

	if (mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0) {
				current_capability |= cap;
				return 0;
			}
		}
	}
	return -1;
}

/*=============================================================================
 * CONVENIENCE FUNCTIONS
 *============================================================================*/

/**
 * tty_putchar - Write a single character to tty
 * @c: Channel
 * @ch: Character to write
 */
static inline int tty_putchar(unsigned c, char ch)
{
	return tty_write(c, &ch, 1);
}

/**
 * tty_getchar - Read a single character from tty
 * @c: Channel
 */
static inline int tty_getchar(unsigned c)
{
	char ch;
	int result = tty_read(c, &ch, 1);
	return (result == 1) ? (unsigned char)ch : -1;
}

/**
 * tty_puts - Write string to tty
 * @c: Channel
 * @s: String to write
 */
static inline int tty_puts(unsigned c, const char *s)
{
	return tty_write(c, (char *)s, strlen(s));
}

/**
 * tty_printf - Formatted output to tty
 * @c: Channel
 * @fmt: Format string
 */
static inline int tty_printf(unsigned c, const char *fmt, ...)
{
	char buf[256];
	va_list args;
	int len;
	
	va_start(args, fmt);
	len = vsprintf(buf, fmt, args);
	va_end(args);
	
	return tty_write(c, buf, len);
}

/**
 * tty_flush - Flush tty output
 * @c: Channel
 */
static inline int tty_flush(unsigned c)
{
	/* Send zero-length write as flush request */
	return tty_write(c, NULL, 0);
}

#endif /* _TTY_H */
