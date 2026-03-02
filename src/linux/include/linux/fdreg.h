/*
* HISTORY
* $Log: fdreg.h,v $
* Revision 1.1 2026/02/14 23:30:45 pedro
* Microkernel version of floppy disk controller definitions.
* Original register addresses preserved for documentation.
* All hardware access now delegated to device server.
* Function prototypes transformed to IPC stubs.
* [2026/02/14 pedro]
*/

/*
* File: linux/fdreg.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* Floppy disk controller definitions for microkernel architecture.
*
* Original Linux 0.11: Direct hardware register access to FDC.
* Microkernel version: All hardware access is delegated to the device server.
* The register addresses are preserved for documentation and compatibility,
* but actual I/O operations are performed by the device server via IPC.
*
* Security: Floppy operations require CAP_DEVICE capability. The device server
* validates all requests and maps them to appropriate hardware access.
*/

#ifndef _FDREG_H
#define _FDREG_H

#include <sys/types.h>
#include <linux/kernel.h>
#include <linux/head.h>

/*=============================================================================
 * ORIGINAL HARDWARE REGISTER ADDRESSES (Preserved for documentation)
 *============================================================================*/

/* Fd controller regs. S&C, about page 340 */
#define FD_STATUS	0x3f4		/* Status register (read) */
#define FD_DATA		0x3f5		/* Data register (read/write) */
#define FD_DOR		0x3f2		/* Digital Output Register */
#define FD_DIR		0x3f7		/* Digital Input Register (read) */
#define FD_DCR		0x3f7		/* Diskette Control Register (write)*/

/* Bits of main status register */
#define STATUS_BUSYMASK	0x0F		/* drive busy mask */
#define STATUS_BUSY	0x10		/* FDC busy */
#define STATUS_DMA	0x20		/* 0- DMA mode */
#define STATUS_DIR	0x40		/* 0- cpu->fdc */
#define STATUS_READY	0x80		/* Data reg ready */

/* Bits of FD_ST0 */
#define ST0_DS		0x03		/* drive select mask */
#define ST0_HA		0x04		/* Head (Address) */
#define ST0_NR		0x08		/* Not Ready */
#define ST0_ECE		0x10		/* Equipment chech error */
#define ST0_SE		0x20		/* Seek end */
#define ST0_INTR	0xC0		/* Interrupt code mask */

/* Bits of FD_ST1 */
#define ST1_MAM		0x01		/* Missing Address Mark */
#define ST1_WP		0x02		/* Write Protect */
#define ST1_ND		0x04		/* No Data - unreadable */
#define ST1_OR		0x10		/* OverRun */
#define ST1_CRC		0x20		/* CRC error in data or addr */
#define ST1_EOC		0x80		/* End Of Cylinder */

/* Bits of FD_ST2 */
#define ST2_MAM		0x01		/* Missing Addess Mark (again) */
#define ST2_BC		0x02		/* Bad Cylinder */
#define ST2_SNS		0x04		/* Scan Not Satisfied */
#define ST2_SEH		0x08		/* Scan Equal Hit */
#define ST2_WC		0x10		/* Wrong Cylinder */
#define ST2_CRC		0x20		/* CRC error in data field */
#define ST2_CM		0x40		/* Control Mark = deleted */

/* Bits of FD_ST3 */
#define ST3_HA		0x04		/* Head (Address) */
#define ST3_TZ		0x10		/* Track Zero signal (1=track 0) */
#define ST3_WP		0x40		/* Write Protect */

/* Values for FD_COMMAND */
#define FD_RECALIBRATE	0x07		/* move to track 0 */
#define FD_SEEK		0x0F		/* seek track */
#define FD_READ		0xE6		/* read with MT, MFM, SKip deleted */
#define FD_WRITE	0xC5		/* write with MT, MFM */
#define FD_SENSEI	0x08		/* Sense Interrupt Status */
#define FD_SPECIFY	0x03		/* specify HUT etc */

/* DMA commands */
#define DMA_READ	0x46
#define DMA_WRITE	0x4A

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_FLOPPY_TICKS_ON	0x1A00	/* Get ticks to floppy on */
#define MSG_FLOPPY_ON		0x1A01	/* Turn floppy motor on */
#define MSG_FLOPPY_OFF		0x1A02	/* Turn floppy motor off */
#define MSG_FLOPPY_SELECT	0x1A03	/* Select drive */
#define MSG_FLOPPY_DESELECT	0x1A04	/* Deselect drive */
#define MSG_FLOPPY_READ		0x1A05	/* Read sector */
#define MSG_FLOPPY_WRITE	0x1A06	/* Write sector */
#define MSG_FLOPPY_SEEK		0x1A07	/* Seek to track */
#define MSG_FLOPPY_RECALIBRATE	0x1A08	/* Recalibrate drive */
#define MSG_FLOPPY_SENSEI	0x1A09	/* Sense interrupt */
#define MSG_FLOPPY_SPECIFY	0x1A0A	/* Specify drive params */
#define MSG_FLOPPY_GET_STATUS	0x1A0B	/* Get drive status */
#define MSG_FLOPPY_FORMAT	0x1A0C	/* Format track */
#define MSG_FLOPPY_REPLY	0x1A0D	/* Reply from device server */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_floppy_request {
	struct mk_msg_header header;
	unsigned int drive;		/* Drive number (0-3) */
	unsigned int command;		/* Floppy command */
	union {
		struct {
			unsigned int cylinder;	/* Cylinder number */
			unsigned int head;	/* Head number */
			unsigned int sector;	/* Sector number */
			unsigned int count;	/* Sector count */
			void *buffer;		/* Data buffer */
		} rw;
		struct {
			unsigned int cylinder;	/* Target cylinder */
		} seek;
		struct {
			unsigned int hut;	/* Head unload time */
			unsigned int hlt;	/* Head load time */
			unsigned int step;	/* Step rate */
		} specify;
	} params;
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
	unsigned int flags;		/* Operation flags */
};

struct msg_floppy_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		unsigned int ticks;	/* Ticks to on */
		struct {
			unsigned char st0;	/* Status register 0 */
			unsigned char st1;	/* Status register 1 */
			unsigned char st2;	/* Status register 2 */
			unsigned char st3;	/* Status register 3 */
			unsigned int cylinder;	/* Current cylinder */
		} status;
		unsigned int bytes;	/* Bytes transferred */
	} data;
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * OPERATION FLAGS
 *============================================================================*/

#define FLOPPY_FLAG_WAIT		0x01	/* Wait for completion */
#define FLOPPY_FLAG_DMA			0x02	/* Use DMA */
#define FLOPPY_FLAG_INT			0x04	/* Use interrupts */
#define FLOPPY_FLAG_MFM			0x08	/* MFM encoding */
#define FLOPPY_FLAG_SKIP		0x10	/* Skip deleted data */
#define FLOPPY_FLAG_MT			0x20	/* Multi-track */

/*=============================================================================
 * CAPABILITY FLAGS
 *============================================================================*/

#define CAP_FLOPPY_READ		0x0010	/* Can read from floppy */
#define CAP_FLOPPY_WRITE	0x0020	/* Can write to floppy */
#define CAP_FLOPPY_CTRL		0x0040	/* Can control floppy motor */
#define CAP_FLOPPY_ADMIN	0x0080	/* Can format/recalibrate */

/*=============================================================================
 * ORIGINAL FUNCTION PROTOTYPES (Now as inline stubs)
 *============================================================================*/

/**
 * ticks_to_floppy_on - Get number of ticks until floppy is ready
 * @nr: Drive number (0-3)
 * 
 * Returns number of ticks needed for floppy motor to spin up.
 */
extern int ticks_to_floppy_on(unsigned int nr);

/**
 * floppy_on - Turn floppy motor on
 * @nr: Drive number (0-3)
 */
extern void floppy_on(unsigned int nr);

/**
 * floppy_off - Turn floppy motor off
 * @nr: Drive number (0-3)
 */
extern void floppy_off(unsigned int nr);

/**
 * floppy_select - Select floppy drive
 * @nr: Drive number (0-3)
 */
extern void floppy_select(unsigned int nr);

/**
 * floppy_deselect - Deselect floppy drive
 * @nr: Drive number (0-3)
 */
extern void floppy_deselect(unsigned int nr);

/*=============================================================================
 * FUNCTION IMPLEMENTATIONS (IPC Stubs)
 *============================================================================*/

/**
 * __floppy_request - Send floppy request to device server
 * @msg_id: Message ID
 * @drive: Drive number
 * @need_reply: Whether to wait for reply
 * 
 * Returns result from device server.
 */
static inline int __floppy_request(unsigned int msg_id, unsigned int drive,
                                     int need_reply)
{
	struct msg_floppy_request msg;
	struct msg_floppy_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	/* Check basic floppy capability */
	if (!(current_capability & CAP_FLOPPY_READ) &&
	    !(current_capability & CAP_FLOPPY_WRITE) &&
	    !(current_capability & CAP_FLOPPY_CTRL)) {
		return -1;
	}

	msg.header.msg_id = msg_id;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = need_reply ? kernel_state->kernel_port : 0;
	msg.header.size = sizeof(msg);

	msg.drive = drive;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	msg.flags = 0;

	result = mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	if (!need_reply)
		return 0;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return reply.result;
}

/**
 * ticks_to_floppy_on - Get ticks until floppy is ready
 * @nr: Drive number
 */
int ticks_to_floppy_on(unsigned int nr)
{
	struct msg_floppy_request msg;
	struct msg_floppy_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (nr > 3)
		return -1;

	msg.header.msg_id = MSG_FLOPPY_TICKS_ON;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.drive = nr;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return (reply.result < 0) ? -1 : reply.data.ticks;
}

/**
 * floppy_on - Turn floppy motor on
 * @nr: Drive number
 */
void floppy_on(unsigned int nr)
{
	if (nr > 3)
		return;

	__floppy_request(MSG_FLOPPY_ON, nr, 0);
}

/**
 * floppy_off - Turn floppy motor off
 * @nr: Drive number
 */
void floppy_off(unsigned int nr)
{
	if (nr > 3)
		return;

	__floppy_request(MSG_FLOPPY_OFF, nr, 0);
}

/**
 * floppy_select - Select floppy drive
 * @nr: Drive number
 */
void floppy_select(unsigned int nr)
{
	if (nr > 3)
		return;

	__floppy_request(MSG_FLOPPY_SELECT, nr, 0);
}

/**
 * floppy_deselect - Deselect floppy drive
 * @nr: Drive number
 */
void floppy_deselect(unsigned int nr)
{
	if (nr > 3)
		return;

	__floppy_request(MSG_FLOPPY_DESELECT, nr, 0);
}

/*=============================================================================
 * ADDITIONAL FLOPPY OPERATIONS
 *============================================================================*/

/**
 * floppy_read_sector - Read a sector from floppy
 * @drive: Drive number
 * @cylinder: Cylinder number
 * @head: Head number
 * @sector: Sector number
 * @buffer: Destination buffer (512 bytes)
 * 
 * Returns number of bytes read, or -1 on error.
 */
static inline int floppy_read_sector(unsigned int drive, unsigned int cylinder,
                                       unsigned int head, unsigned int sector,
                                       void *buffer)
{
	struct msg_floppy_request msg;
	struct msg_floppy_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_FLOPPY_READ))
		return -1;

	msg.header.msg_id = MSG_FLOPPY_READ;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.drive = drive;
	msg.command = FD_READ;
	msg.params.rw.cylinder = cylinder;
	msg.params.rw.head = head;
	msg.params.rw.sector = sector;
	msg.params.rw.count = 1;
	msg.params.rw.buffer = buffer;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	msg.flags = FLOPPY_FLAG_MFM | FLOPPY_FLAG_WAIT;

	result = mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return (reply.result < 0) ? -1 : reply.data.bytes;
}

/**
 * floppy_write_sector - Write a sector to floppy
 * @drive: Drive number
 * @cylinder: Cylinder number
 * @head: Head number
 * @sector: Sector number
 * @buffer: Source buffer (512 bytes)
 * 
 * Returns number of bytes written, or -1 on error.
 */
static inline int floppy_write_sector(unsigned int drive, unsigned int cylinder,
                                        unsigned int head, unsigned int sector,
                                        const void *buffer)
{
	struct msg_floppy_request msg;
	struct msg_floppy_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_FLOPPY_WRITE))
		return -1;

	msg.header.msg_id = MSG_FLOPPY_WRITE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.drive = drive;
	msg.command = FD_WRITE;
	msg.params.rw.cylinder = cylinder;
	msg.params.rw.head = head;
	msg.params.rw.sector = sector;
	msg.params.rw.count = 1;
	msg.params.rw.buffer = (void *)buffer;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	msg.flags = FLOPPY_FLAG_MFM | FLOPPY_FLAG_WAIT;

	result = mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return (reply.result < 0) ? -1 : reply.data.bytes;
}

/**
 * floppy_seek - Seek to cylinder
 * @drive: Drive number
 * @cylinder: Target cylinder
 * 
 * Returns 0 on success, -1 on error.
 */
static inline int floppy_seek(unsigned int drive, unsigned int cylinder)
{
	struct msg_floppy_request msg;
	struct msg_floppy_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_FLOPPY_CTRL))
		return -1;

	msg.header.msg_id = MSG_FLOPPY_SEEK;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.drive = drive;
	msg.command = FD_SEEK;
	msg.params.seek.cylinder = cylinder;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return reply.result;
}

/**
 * floppy_recalibrate - Recalibrate drive (seek to track 0)
 * @drive: Drive number
 * 
 * Returns 0 on success, -1 on error.
 */
static inline int floppy_recalibrate(unsigned int drive)
{
	struct msg_floppy_request msg;
	struct msg_floppy_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_FLOPPY_ADMIN))
		return -1;

	msg.header.msg_id = MSG_FLOPPY_RECALIBRATE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.drive = drive;
	msg.command = FD_RECALIBRATE;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return reply.result;
}

/**
 * floppy_sense_interrupt - Get interrupt status
 * @drive: Drive number
 * @st0: Output status register 0
 * @cylinder: Output current cylinder
 * 
 * Returns 0 on success, -1 on error.
 */
static inline int floppy_sense_interrupt(unsigned int drive,
                                           unsigned char *st0,
                                           unsigned int *cylinder)
{
	struct msg_floppy_request msg;
	struct msg_floppy_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	msg.header.msg_id = MSG_FLOPPY_SENSEI;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.drive = drive;
	msg.command = FD_SENSEI;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	if (reply.result == 0) {
		if (st0) *st0 = reply.data.status.st0;
		if (cylinder) *cylinder = reply.data.status.cylinder;
	}

	return reply.result;
}

/**
 * floppy_specify - Set drive timing parameters
 * @drive: Drive number
 * @hut: Head unload time
 * @hlt: Head load time
 * @step: Step rate
 * 
 * Returns 0 on success, -1 on error.
 */
static inline int floppy_specify(unsigned int drive,
                                   unsigned int hut,
                                   unsigned int hlt,
                                   unsigned int step)
{
	struct msg_floppy_request msg;
	
	if (!(current_capability & CAP_FLOPPY_ADMIN))
		return -1;

	msg.header.msg_id = MSG_FLOPPY_SPECIFY;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);

	msg.drive = drive;
	msg.command = FD_SPECIFY;
	msg.params.specify.hut = hut;
	msg.params.specify.hlt = hlt;
	msg.params.specify.step = step;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	return mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
}

/**
 * floppy_get_status - Get drive status
 * @drive: Drive number
 * @st3: Output status register 3
 * 
 * Returns 0 on success, -1 on error.
 */
static inline int floppy_get_status(unsigned int drive, unsigned char *st3)
{
	struct msg_floppy_request msg;
	struct msg_floppy_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	msg.header.msg_id = MSG_FLOPPY_GET_STATUS;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.drive = drive;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	if (reply.result == 0 && st3)
		*st3 = reply.data.status.st3;

	return reply.result;
}

/*=============================================================================
 * CAPABILITY REQUEST FUNCTION
 *============================================================================*/

static inline int request_floppy_capability(unsigned int cap)
{
	struct msg_cap_request msg;
	struct msg_cap_reply reply;
	unsigned int reply_size = sizeof(reply);

	msg.header.msg_id = MSG_CAP_REQUEST_FLOPPY;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.task_id = kernel_state->current_task;
	msg.requested_cap = cap;

	if (mk_msg_send(kernel_state->device_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0) {
				current_capability |= cap;
				return 0;
			}
		}
	}
	return -1;
}

#endif /* _FDREG_H */
