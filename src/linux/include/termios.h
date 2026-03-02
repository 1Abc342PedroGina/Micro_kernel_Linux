/*
* HISTORY
* $Log: termios.h,v $
* Revision 1.1 2026/02/14 06:30:45 pedro
* Microkernel version of terminal I/O control.
* All termios functions now send IPC messages to terminal server.
* struct termios/termio preserved for compatibility.
* TIOC* ioctls become message codes to terminal server.
* [2026/02/14 pedro]
*/

/*
* File: termios.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* Terminal I/O control for microkernel architecture.
*
* Original Linux 0.11: Direct ioctl calls to terminal driver.
* Microkernel version: All terminal operations send IPC messages to
* the terminal server, which manages virtual terminals, line disciplines,
* and hardware access.
*
* Security: Terminal operations require CAP_TTY capability. The terminal
* server validates that the task has access to the specified terminal
* (controlling terminal, permissions, etc.).
*/

#ifndef _TERMIOS_H
#define _TERMIOS_H

#include <sys/types.h>
#include <linux/kernel.h>
#include <linux/head.h>

/* Message codes for terminal operations */
#define MSG_TTY_TCGETS		0x0E00	/* Get termios */
#define MSG_TTY_TCSETS		0x0E01	/* Set termios now */
#define MSG_TTY_TCSETSW		0x0E02	/* Set termios when drained */
#define MSG_TTY_TCSETSF		0x0E03	/* Set termios after flush */
#define MSG_TTY_TCGETA		0x0E04	/* Get termio (compat) */
#define MSG_TTY_TCSETA		0x0E05	/* Set termio now */
#define MSG_TTY_TCSETAW		0x0E06	/* Set termio when drained */
#define MSG_TTY_TCSETAF		0x0E07	/* Set termio after flush */
#define MSG_TTY_TCSBRK		0x0E08	/* Send break */
#define MSG_TTY_TCXONC		0x0E09	/* Start/stop control */
#define MSG_TTY_TCFLSH		0x0E0A	/* Flush buffers */
#define MSG_TTY_TIOCEXCL	0x0E0B	/* Set exclusive use */
#define MSG_TTY_TIOCNXCL	0x0E0C	/* Clear exclusive use */
#define MSG_TTY_TIOCSCTTY	0x0E0D	/* Set controlling terminal */
#define MSG_TTY_TIOCGPGRP	0x0E0E	/* Get process group */
#define MSG_TTY_TIOCSPGRP	0x0E0F	/* Set process group */
#define MSG_TTY_TIOCOUTQ	0x0E10	/* Get output queue size */
#define MSG_TTY_TIOCSTI		0x0E11	/* Simulate input */
#define MSG_TTY_TIOCGWINSZ	0x0E12	/* Get window size */
#define MSG_TTY_TIOCSWINSZ	0x0E13	/* Set window size */
#define MSG_TTY_TIOCMGET	0x0E14	/* Get modem lines */
#define MSG_TTY_TIOCMBIS	0x0E15	/* Set modem bits */
#define MSG_TTY_TIOCMBIC	0x0E16	/* Clear modem bits */
#define MSG_TTY_TIOCMSET	0x0E17	/* Set all modem lines */
#define MSG_TTY_TIOCGSOFTCAR	0x0E18	/* Get software carrier */
#define MSG_TTY_TIOCSSOFTCAR	0x0E19	/* Set software carrier */
#define MSG_TTY_TIOCINQ		0x0E1A	/* Get input queue size */
#define MSG_TTY_CFGETISPEED	0x0E1B	/* Get input speed */
#define MSG_TTY_CFGETOSPEED	0x0E1C	/* Get output speed */
#define MSG_TTY_CFSETISPEED	0x0E1D	/* Set input speed */
#define MSG_TTY_CFSETOSPEED	0x0E1E	/* Set output speed */
#define MSG_TTY_TCDRAIN		0x0E1F	/* Drain output */
#define MSG_TTY_REPLY		0x0E20	/* Reply from terminal server */

/* Terminal operation message structures */
struct msg_tty_getattr {
	struct mk_msg_header header;
	int fildes;				/* Terminal file descriptor */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_tty_setattr {
	struct mk_msg_header header;
	int fildes;				/* Terminal file descriptor */
	int optional_actions;			/* TCSANOW, TCSADRAIN, TCSAFLUSH */
	struct termios termios;			/* New termios data */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_tty_ioctl {
	struct mk_msg_header header;
	int fildes;				/* Terminal file descriptor */
	int request;				/* TIOC* request */
	void *arg;				/* Argument (depends on request) */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_tty_cfspeed {
	struct mk_msg_header header;
	int fildes;				/* Terminal file descriptor */
	speed_t speed;				/* New speed */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_tty_flow {
	struct mk_msg_header header;
	int fildes;				/* Terminal file descriptor */
	int action;				/* TCOOFF, TCOON, TCIOFF, TCION */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_tty_flush {
	struct mk_msg_header header;
	int fildes;				/* Terminal file descriptor */
	int queue_selector;			/* TCIFLUSH, TCOFLUSH, TCIOFLUSH */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_tty_break {
	struct mk_msg_header header;
	int fildes;				/* Terminal file descriptor */
	int duration;				/* Break duration */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_tty_reply {
	struct mk_msg_header header;
	int result;				/* Result code */
	union {
		struct termios termios;	/* For getattr */
		struct termio termio;		/* For compat */
		struct winsize winsize;	/* For window size */
		int pgrp;			/* For process group */
		int queue_size;			/* For queue queries */
		int modem_lines;		/* For modem queries */
		speed_t speed;			/* For speed queries */
	} data;
	capability_t granted_caps;		/* New capabilities granted */
};

/* Original definitions preserved exactly for compatibility */

#define TTY_BUF_SIZE 1024

/* 0x54 is just a magic number to make these relatively uniqe ('T') */
#define TCGETS		0x5401
#define TCSETS		0x5402
#define TCSETSW		0x5403
#define TCSETSF		0x5404
#define TCGETA		0x5405
#define TCSETA		0x5406
#define TCSETAW		0x5407
#define TCSETAF		0x5408
#define TCSBRK		0x5409
#define TCXONC		0x540A
#define TCFLSH		0x540B
#define TIOCEXCL	0x540C
#define TIOCNXCL	0x540D
#define TIOCSCTTY	0x540E
#define TIOCGPGRP	0x540F
#define TIOCSPGRP	0x5410
#define TIOCOUTQ	0x5411
#define TIOCSTI		0x5412
#define TIOCGWINSZ	0x5413
#define TIOCSWINSZ	0x5414
#define TIOCMGET	0x5415
#define TIOCMBIS	0x5416
#define TIOCMBIC	0x5417
#define TIOCMSET	0x5418
#define TIOCGSOFTCAR	0x5419
#define TIOCSSOFTCAR	0x541A
#define TIOCINQ		0x541B

struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

#define NCC 8
struct termio {
	unsigned short c_iflag;		/* input mode flags */
	unsigned short c_oflag;		/* output mode flags */
	unsigned short c_cflag;		/* control mode flags */
	unsigned short c_lflag;		/* local mode flags */
	unsigned char c_line;		/* line discipline */
	unsigned char c_cc[NCC];	/* control characters */
};

#define NCCS 17
struct termios {
	unsigned long c_iflag;		/* input mode flags */
	unsigned long c_oflag;		/* output mode flags */
	unsigned long c_cflag;		/* control mode flags */
	unsigned long c_lflag;		/* local mode flags */
	unsigned char c_line;		/* line discipline */
	unsigned char c_cc[NCCS];	/* control characters */
};

/* c_cc characters */
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSWTC 7
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VEOL 11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT 15
#define VEOL2 16

/* c_iflag bits */
#define IGNBRK	0000001
#define BRKINT	0000002
#define IGNPAR	0000004
#define PARMRK	0000010
#define INPCK	0000020
#define ISTRIP	0000040
#define INLCR	0000100
#define IGNCR	0000200
#define ICRNL	0000400
#define IUCLC	0001000
#define IXON	0002000
#define IXANY	0004000
#define IXOFF	0010000
#define IMAXBEL	0020000

/* c_oflag bits */
#define OPOST	0000001
#define OLCUC	0000002
#define ONLCR	0000004
#define OCRNL	0000010
#define ONOCR	0000020
#define ONLRET	0000040
#define OFILL	0000100
#define OFDEL	0000200
#define NLDLY	0000400
#define   NL0	0000000
#define   NL1	0000400
#define CRDLY	0003000
#define   CR0	0000000
#define   CR1	0001000
#define   CR2	0002000
#define   CR3	0003000
#define TABDLY	0014000
#define   TAB0	0000000
#define   TAB1	0004000
#define   TAB2	0010000
#define   TAB3	0014000
#define   XTABS	0014000
#define BSDLY	0020000
#define   BS0	0000000
#define   BS1	0020000
#define VTDLY	0040000
#define   VT0	0000000
#define   VT1	0040000
#define FFDLY	0040000
#define   FF0	0000000
#define   FF1	0040000

/* c_cflag bit meaning */
#define CBAUD	0000017
#define  B0	0000000		/* hang up */
#define  B50	0000001
#define  B75	0000002
#define  B110	0000003
#define  B134	0000004
#define  B150	0000005
#define  B200	0000006
#define  B300	0000007
#define  B600	0000010
#define  B1200	0000011
#define  B1800	0000012
#define  B2400	0000013
#define  B4800	0000014
#define  B9600	0000015
#define  B19200	0000016
#define  B38400	0000017
#define EXTA B19200
#define EXTB B38400
#define CSIZE	0000060
#define   CS5	0000000
#define   CS6	0000020
#define   CS7	0000040
#define   CS8	0000060
#define CSTOPB	0000100
#define CREAD	0000200
#define CPARENB	0000400
#define CPARODD	0001000
#define HUPCL	0002000
#define CLOCAL	0004000
#define CIBAUD	03600000		/* input baud rate (not used) */
#define CRTSCTS	020000000000		/* flow control */

#define PARENB CPARENB
#define PARODD CPARODD

/* c_lflag bits */
#define ISIG	0000001
#define ICANON	0000002
#define XCASE	0000004
#define ECHO	0000010
#define ECHOE	0000020
#define ECHOK	0000040
#define ECHONL	0000100
#define NOFLSH	0000200
#define TOSTOP	0000400
#define ECHOCTL	0001000
#define ECHOPRT	0002000
#define ECHOKE	0004000
#define FLUSHO	0010000
#define PENDIN	0040000
#define IEXTEN	0100000

/* modem lines */
#define TIOCM_LE	0x001
#define TIOCM_DTR	0x002
#define TIOCM_RTS	0x004
#define TIOCM_ST	0x008
#define TIOCM_SR	0x010
#define TIOCM_CTS	0x020
#define TIOCM_CAR	0x040
#define TIOCM_RNG	0x080
#define TIOCM_DSR	0x100
#define TIOCM_CD	TIOCM_CAR
#define TIOCM_RI	TIOCM_RNG

/* tcflow() and TCXONC use these */
#define	TCOOFF		0
#define	TCOON		1
#define	TCIOFF		2
#define	TCION		3

/* tcflush() and TCFLSH use these */
#define	TCIFLUSH	0
#define	TCOFLUSH	1
#define	TCIOFLUSH	2

/* tcsetattr uses these */
#define	TCSANOW		0
#define	TCSADRAIN	1
#define	TCSAFLUSH	2

typedef int speed_t;

/* Capability for terminal operations */
#define CAP_TTY		0x0040	/* Terminal capability */

/* Public function prototypes */
extern speed_t cfgetispeed(struct termios *termios_p);
extern speed_t cfgetospeed(struct termios *termios_p);
extern int cfsetispeed(struct termios *termios_p, speed_t speed);
extern int cfsetospeed(struct termios *termios_p, speed_t speed);
extern int tcdrain(int fildes);
extern int tcflow(int fildes, int action);
extern int tcflush(int fildes, int queue_selector);
extern int tcgetattr(int fildes, struct termios *termios_p);
extern int tcsendbreak(int fildes, int duration);
extern int tcsetattr(int fildes, int optional_actions,
	struct termios *termios_p);

/* Internal helper function */
static inline int __tty_request(int fildes, int request, void *arg, int msg_id)
{
	struct msg_tty_ioctl msg;
	struct msg_tty_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	/* Check TTY capability */
	if (!(current_capability & CAP_TTY)) {
		if (request_tty_capability() < 0)
			return -1;  /* EPERM */
	}

	msg.header.msg_id = msg_id;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.fildes = fildes;
	msg.request = request;
	msg.arg = arg;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	/* Copy out any returned data */
	if (reply.result == 0 && arg) {
		switch (request) {
			case TCGETS:
			case TCGETA:
				*(struct termios *)arg = reply.data.termios;
				break;
			case TIOCGWINSZ:
				*(struct winsize *)arg = reply.data.winsize;
				break;
			case TIOCGPGRP:
				*(int *)arg = reply.data.pgrp;
				break;
			case TIOCOUTQ:
			case TIOCINQ:
				*(int *)arg = reply.data.queue_size;
				break;
			case TIOCMGET:
				*(int *)arg = reply.data.modem_lines;
				break;
		}
	}

	/* Update capabilities if granted */
	if (reply.granted_caps)
		current_capability |= reply.granted_caps;

	return reply.result;
}

/* Public function implementations */

int tcgetattr(int fildes, struct termios *termios_p)
{
	if (!termios_p)
		return -1;  /* EFAULT */
	
	return __tty_request(fildes, TCGETS, termios_p, MSG_TTY_TCGETS);
}

int tcsetattr(int fildes, int optional_actions, struct termios *termios_p)
{
	struct msg_tty_setattr msg;
	struct msg_tty_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!termios_p)
		return -1;

	if (!(current_capability & CAP_TTY))
		return -1;

	msg.header.msg_id = MSG_TTY_TCSETS + optional_actions;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.fildes = fildes;
	msg.optional_actions = optional_actions;
	msg.termios = *termios_p;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return reply.result;
}

speed_t cfgetispeed(struct termios *termios_p)
{
	if (!termios_p)
		return B0;
	
	/* Speed is encoded in c_cflag */
	return termios_p->c_cflag & CBAUD;
}

speed_t cfgetospeed(struct termios *termios_p)
{
	if (!termios_p)
		return B0;
	
	return termios_p->c_cflag & CBAUD;
}

int cfsetispeed(struct termios *termios_p, speed_t speed)
{
	if (!termios_p)
		return -1;
	
	/* Only set if speed is valid */
	if (speed < B0 || speed > B38400)
		return -1;
	
	termios_p->c_cflag = (termios_p->c_cflag & ~CBAUD) | speed;
	return 0;
}

int cfsetospeed(struct termios *termios_p, speed_t speed)
{
	/* Same as input speed for now */
	return cfsetispeed(termios_p, speed);
}

int tcdrain(int fildes)
{
	return __tty_request(fildes, 0, NULL, MSG_TTY_TCDRAIN);
}

int tcflow(int fildes, int action)
{
	struct msg_tty_flow msg;
	struct msg_tty_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_TTY))
		return -1;

	msg.header.msg_id = MSG_TTY_TCXONC;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.fildes = fildes;
	msg.action = action;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	return result;
}

int tcflush(int fildes, int queue_selector)
{
	struct msg_tty_flush msg;
	struct msg_tty_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_TTY))
		return -1;

	msg.header.msg_id = MSG_TTY_TCFLSH;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.fildes = fildes;
	msg.queue_selector = queue_selector;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	return result;
}

int tcsendbreak(int fildes, int duration)
{
	struct msg_tty_break msg;
	struct msg_tty_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_TTY))
		return -1;

	msg.header.msg_id = MSG_TTY_TCSBRK;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.fildes = fildes;
	msg.duration = duration;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	return result;
}

/* TIOC* ioctl functions */

int tiocgetpgrp(int fildes)
{
	int pgrp;
	int result = __tty_request(fildes, TIOCGPGRP, &pgrp, MSG_TTY_TIOCGPGRP);
	return (result < 0) ? result : pgrp;
}

int tiocspgrp(int fildes, int pgrp)
{
	return __tty_request(fildes, TIOCSPGRP, &pgrp, MSG_TTY_TIOCSPGRP);
}

int tiocgwinsz(int fildes, struct winsize *ws)
{
	return __tty_request(fildes, TIOCGWINSZ, ws, MSG_TTY_TIOCGWINSZ);
}

int tiocswinsz(int fildes, struct winsize *ws)
{
	return __tty_request(fildes, TIOCSWINSZ, ws, MSG_TTY_TIOCSWINSZ);
}

int tiocmget(int fildes)
{
	int lines;
	int result = __tty_request(fildes, TIOCMGET, &lines, MSG_TTY_TIOCMGET);
	return (result < 0) ? result : lines;
}

int tiocmbis(int fildes, int bits)
{
	return __tty_request(fildes, TIOCMBIS, &bits, MSG_TTY_TIOCMBIS);
}

int tiocmbic(int fildes, int bits)
{
	return __tty_request(fildes, TIOCMBIC, &bits, MSG_TTY_TIOCMBIC);
}

int tiocmset(int fildes, int bits)
{
	return __tty_request(fildes, TIOCMSET, &bits, MSG_TTY_TIOCMSET);
}

int tiocinq(int fildes)
{
	int size;
	int result = __tty_request(fildes, TIOCINQ, &size, MSG_TTY_TIOCINQ);
	return (result < 0) ? result : size;
}

int tiocoutq(int fildes)
{
	int size;
	int result = __tty_request(fildes, TIOCOUTQ, &size, MSG_TTY_TIOCOUTQ);
	return (result < 0) ? result : size;
}

int tiocsctty(int fildes)
{
	return __tty_request(fildes, TIOCSCTTY, NULL, MSG_TTY_TIOCSCTTY);
}

int tiocexcl(int fildes)
{
	return __tty_request(fildes, TIOCEXCL, NULL, MSG_TTY_TIOCEXCL);
}

int tiocnxcl(int fildes)
{
	return __tty_request(fildes, TIOCNXCL, NULL, MSG_TTY_TIOCNXCL);
}

int tiocsti(int fildes, char ch)
{
	return __tty_request(fildes, TIOCSTI, &ch, MSG_TTY_TIOCSTI);
}

int tiocgsoftcar(int fildes)
{
	int flag;
	int result = __tty_request(fildes, TIOCGSOFTCAR, &flag, MSG_TTY_TIOCGSOFTCAR);
	return (result < 0) ? result : flag;
}

int tiocsoftcar(int fildes, int flag)
{
	return __tty_request(fildes, TIOCSSOFTCAR, &flag, MSG_TTY_TIOCSSOFTCAR);
}

/* Compatibility with older termio interface */
int tcgetattr_old(int fildes, struct termio *termio_p)
{
	struct termios termios;
	int result = tcgetattr(fildes, &termios);
	
	if (result == 0 && termio_p) {
		/* Convert termios to termio */
		termio_p->c_iflag = termios.c_iflag;
		termio_p->c_oflag = termios.c_oflag;
		termio_p->c_cflag = termios.c_cflag;
		termio_p->c_lflag = termios.c_lflag;
		termio_p->c_line = termios.c_line;
		memcpy(termio_p->c_cc, termios.c_cc, NCC);
	}
	
	return result;
}

int tcsetattr_old(int fildes, int optional_actions, struct termio *termio_p)
{
	struct termios termios;
	
	if (termio_p) {
		/* Convert termio to termios */
		termios.c_iflag = termio_p->c_iflag;
		termios.c_oflag = termio_p->c_oflag;
		termios.c_cflag = termio_p->c_cflag;
		termios.c_lflag = termio_p->c_lflag;
		termios.c_line = termio_p->c_line;
		memcpy(termios.c_cc, termio_p->c_cc, NCC);
		/* Pad remaining with defaults */
		for (int i = NCC; i < NCCS; i++)
			termios.c_cc[i] = 0;
	}
	
	return tcsetattr(fildes, optional_actions, termio_p ? &termios : NULL);
}

/* Capability request */
static inline int request_tty_capability(void)
{
	struct msg_cap_request msg;
	struct msg_cap_reply reply;
	unsigned int reply_size = sizeof(reply);

	msg.header.msg_id = MSG_CAP_REQUEST_TTY;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.task_id = kernel_state->current_task;
	msg.requested_cap = CAP_TTY;

	if (mk_msg_send(kernel_state->tty_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0) {
				current_capability |= CAP_TTY;
				return 0;
			}
		}
	}
	return -1;
}

#endif /* _TERMIOS_H */
