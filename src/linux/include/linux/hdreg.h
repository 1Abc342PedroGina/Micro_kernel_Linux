/*
* HISTORY
* $Log: hdreg.h,v $
* Revision 1.1 2026/02/14 17:30:45 pedro
* Microkernel version of hard disk register definitions.
* Original port addresses preserved for documentation.
* All hardware access now delegated to device server.
* partition structure preserved for compatibility.
* [2026/02/14 pedro]
*/

/*
* File: linux/hdreg.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* Hard disk controller definitions for microkernel architecture.
*
* Original Linux 0.11: Direct hardware register access to IDE controller.
* Microkernel version: All hardware access is delegated to the device server.
* The register addresses are preserved for documentation and compatibility,
* but actual I/O operations are performed by the device server via IPC.
*
* Security: Disk operations require CAP_DEVICE capability. The device server
* validates all requests and maps them to appropriate hardware access.
*/

#ifndef _HDREG_H
#define _HDREG_H

#include <sys/types.h>
#include <linux/kernel.h>
#include <linux/head.h>

/*=============================================================================
 * ORIGINAL HARDWARE REGISTER ADDRESSES (Preserved for documentation)
 *============================================================================*/

/* Hd controller regs. Ref: IBM AT Bios-listing */
#define HD_DATA		0x1f0	/* _CTL when writing */
#define HD_ERROR	0x1f1	/* see err-bits */
#define HD_NSECTOR	0x1f2	/* nr of sectors to read/write */
#define HD_SECTOR	0x1f3	/* starting sector */
#define HD_LCYL		0x1f4	/* starting cylinder */
#define HD_HCYL		0x1f5	/* high byte of starting cyl */
#define HD_CURRENT	0x1f6	/* 101dhhhh , d=drive, hhhh=head */
#define HD_STATUS	0x1f7	/* see status-bits */
#define HD_PRECOMP HD_ERROR	/* same io address, read=error, write=precomp */
#define HD_COMMAND HD_STATUS	/* same io address, read=status, write=cmd */

#define HD_CMD		0x3f6

/* Bits of HD_STATUS */
#define ERR_STAT	0x01
#define INDEX_STAT	0x02
#define ECC_STAT	0x04	/* Corrected error */
#define DRQ_STAT	0x08
#define SEEK_STAT	0x10
#define WRERR_STAT	0x20
#define READY_STAT	0x40
#define BUSY_STAT	0x80

/* Values for HD_COMMAND */
#define WIN_RESTORE		0x10
#define WIN_READ		0x20
#define WIN_WRITE		0x30
#define WIN_VERIFY		0x40
#define WIN_FORMAT		0x50
#define WIN_INIT		0x60
#define WIN_SEEK 		0x70
#define WIN_DIAGNOSE		0x90
#define WIN_SPECIFY		0x91

/* Bits for HD_ERROR */
#define MARK_ERR	0x01	/* Bad address mark ? */
#define TRK0_ERR	0x02	/* couldn't find track 0 */
#define ABRT_ERR	0x04	/* ? */
#define ID_ERR		0x10	/* ? */
#define ECC_ERR		0x40	/* ? */
#define	BBD_ERR		0x80	/* ? */

/*=============================================================================
 * ORIGINAL PARTITION STRUCTURE (Preserved for compatibility)
 *============================================================================*/

struct partition {
	unsigned char boot_ind;		/* 0x80 - active (unused) */
	unsigned char head;		/* ? */
	unsigned char sector;		/* ? */
	unsigned char cyl;		/* ? */
	unsigned char sys_ind;		/* ? */
	unsigned char end_head;		/* ? */
	unsigned char end_sector;	/* ? */
	unsigned char end_cyl;		/* ? */
	unsigned int start_sect;	/* starting sector counting from 0 */
	unsigned int nr_sects;		/* nr of sectors in partition */
};

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_HD_READ		0x1400	/* Read sectors */
#define MSG_HD_WRITE		0x1401	/* Write sectors */
#define MSG_HD_VERIFY		0x1402	/* Verify sectors */
#define MSG_HD_SEEK		0x1403	/* Seek to cylinder */
#define MSG_HD_FORMAT		0x1404	/* Format track */
#define MSG_HD_RESTORE		0x1405	/* Restore (recalibrate) */
#define MSG_HD_INIT		0x1406	/* Initialize drive */
#define MSG_HD_DIAGNOSE		0x1407	/* Run diagnostics */
#define MSG_HD_SPECIFY		0x1408	/* Specify drive parameters */
#define MSG_HD_GET_STATUS	0x1409	/* Get drive status */
#define MSG_HD_GET_INFO		0x140A	/* Get drive info */
#define MSG_HD_GET_PARTITIONS	0x140B	/* Read partition table */
#define MSG_HD_REPLY		0x140C	/* Reply from device server */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_hd_rw {
	struct mk_msg_header header;
	unsigned int drive;		/* Drive number (0=master, 1=slave) */
	unsigned int sector;		/* Starting sector (LBA or CHS) */
	unsigned int count;		/* Number of sectors */
	void *buffer;			/* Data buffer */
	unsigned int flags;		/* Operation flags */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_hd_command {
	struct mk_msg_header header;
	unsigned int drive;		/* Drive number */
	unsigned int command;		/* Command code (WIN_*) */
	unsigned int param1;		/* Command parameter 1 */
	unsigned int param2;		/* Command parameter 2 */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_hd_get_info {
	struct mk_msg_header header;
	unsigned int drive;		/* Drive number */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_hd_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		struct {
			unsigned int status;	/* Status register */
			unsigned int error;	/* Error register */
		} status;
		struct {
			unsigned int cylinders;	/* Number of cylinders */
			unsigned int heads;		/* Number of heads */
			unsigned int sectors;		/* Sectors per track */
			unsigned int size;		/* Total size in sectors */
		} drive_info;
		struct partition partitions[4];	/* Partition table */
		unsigned int bytes;		/* Bytes transferred */
	} data;
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * CAPABILITY FLAGS FOR DISK OPERATIONS
 *============================================================================*/

#define CAP_DISK_READ		0x0100	/* Can read from disk */
#define CAP_DISK_WRITE		0x0200	/* Can write to disk */
#define CAP_DISK_ADMIN		0x0400	/* Administrative ops (format, etc) */
#define CAP_DISK_INFO		0x0800	/* Can get disk info */

/*=============================================================================
 * OPERATION FLAGS
 *============================================================================*/

#define HD_FLAG_LBA		0x01	/* Use LBA addressing */
#define HD_FLAG_CHS		0x02	/* Use CHS addressing */
#define HD_FLAG_NOWAIT		0x04	/* Don't wait for completion */
#define HD_FLAG_DMA		0x08	/* Use DMA if available */
#define HD_FLAG_VERIFY		0x10	/* Verify after write */
#define HD_FLAG_CACHE		0x20	/* Use disk cache */

/*=============================================================================
 * DRIVE NUMBERS
 *============================================================================*/

#define HD_DRIVE_MASTER		0	/* Primary master */
#define HD_DRIVE_SLAVE		1	/* Primary slave */
#define HD_DRIVE_SECOND_MASTER	2	/* Secondary master */
#define HD_DRIVE_SECOND_SLAVE	3	/* Secondary slave */
#define HD_MAX_DRIVES		4

/*=============================================================================
 * PUBLIC FUNCTION PROTOTYPES
 *============================================================================*/

/* High-level disk operations */
int hd_read(unsigned int drive, unsigned int sector, 
            void *buffer, unsigned int count);
int hd_write(unsigned int drive, unsigned int sector,
             const void *buffer, unsigned int count);
int hd_verify(unsigned int drive, unsigned int sector, unsigned int count);
int hd_seek(unsigned int drive, unsigned int cylinder);

/* Low-level commands */
int hd_command(unsigned int drive, unsigned int cmd,
               unsigned int param1, unsigned int param2);
int hd_restore(unsigned int drive);
int hd_format_track(unsigned int drive, unsigned int cylinder, unsigned int head);
int hd_diagnose(unsigned int drive);
int hd_specify(unsigned int drive, unsigned int params);

/* Information functions */
int hd_get_status(unsigned int drive, unsigned int *status, unsigned int *error);
int hd_get_drive_info(unsigned int drive, struct hd_drive_info *info);
int hd_get_partitions(unsigned int drive, struct partition *partitions);

/*=============================================================================
 * INTERNAL HELPER FUNCTIONS
 *============================================================================*/

/**
 * __hd_request - Send disk request to device server
 * @msg_id: Message ID
 * @drive: Drive number
 * @sector: Sector number
 * @buffer: Data buffer
 * @count: Sector count
 * @flags: Operation flags
 * @need_reply: Whether to wait for reply
 * 
 * Returns result from device server.
 */
static inline int __hd_request(unsigned int msg_id, unsigned int drive,
                                unsigned int sector, void *buffer,
                                unsigned int count, unsigned int flags,
                                int need_reply)
{
	struct msg_hd_rw msg;
	struct msg_hd_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	/* Check capability based on operation */
	if ((msg_id == MSG_HD_READ) && !(current_capability & CAP_DISK_READ))
		return -1;
	if ((msg_id == MSG_HD_WRITE) && !(current_capability & CAP_DISK_WRITE))
		return -1;

	msg.header.msg_id = msg_id;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = need_reply ? kernel_state->kernel_port : 0;
	msg.header.size = sizeof(msg);

	msg.drive = drive;
	msg.sector = sector;
	msg.count = count;
	msg.buffer = buffer;
	msg.flags = flags;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
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

/*=============================================================================
 * FUNCTION IMPLEMENTATIONS
 *============================================================================*/

/**
 * hd_read - Read sectors from disk
 * @drive: Drive number (0=master, 1=slave)
 * @sector: Starting sector number (LBA)
 * @buffer: Destination buffer
 * @count: Number of sectors to read
 * 
 * Returns number of bytes read, or -1 on error.
 */
int hd_read(unsigned int drive, unsigned int sector,
            void *buffer, unsigned int count)
{
	if (drive >= HD_MAX_DRIVES)
		return -1;
	
	return __hd_request(MSG_HD_READ, drive, sector, buffer,
	                    count, HD_FLAG_LBA, 1);
}

/**
 * hd_write - Write sectors to disk
 * @drive: Drive number
 * @sector: Starting sector number
 * @buffer: Source buffer
 * @count: Number of sectors to write
 * 
 * Returns number of bytes written, or -1 on error.
 */
int hd_write(unsigned int drive, unsigned int sector,
             const void *buffer, unsigned int count)
{
	if (drive >= HD_MAX_DRIVES)
		return -1;
	
	return __hd_request(MSG_HD_WRITE, drive, sector, (void *)buffer,
	                    count, HD_FLAG_LBA, 1);
}

/**
 * hd_verify - Verify sectors
 * @drive: Drive number
 * @sector: Starting sector
 * @count: Number of sectors
 * 
 * Returns 0 on success, -1 on error.
 */
int hd_verify(unsigned int drive, unsigned int sector, unsigned int count)
{
	if (drive >= HD_MAX_DRIVES)
		return -1;
	
	return __hd_request(MSG_HD_VERIFY, drive, sector, NULL,
	                    count, HD_FLAG_LBA, 1);
}

/**
 * hd_seek - Seek to cylinder
 * @drive: Drive number
 * @cylinder: Cylinder number
 * 
 * Returns 0 on success, -1 on error.
 */
int hd_seek(unsigned int drive, unsigned int cylinder)
{
	struct msg_hd_command msg;
	struct msg_hd_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (drive >= HD_MAX_DRIVES)
		return -1;

	msg.header.msg_id = MSG_HD_SEEK;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.drive = drive;
	msg.command = WIN_SEEK;
	msg.param1 = cylinder;
	msg.param2 = 0;
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
 * hd_restore - Recalibrate drive (seek to cylinder 0)
 * @drive: Drive number
 * 
 * Returns 0 on success, -1 on error.
 */
int hd_restore(unsigned int drive)
{
	struct msg_hd_command msg;
	
	if (!(current_capability & CAP_DISK_ADMIN))
		return -1;

	msg.header.msg_id = MSG_HD_RESTORE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);

	msg.drive = drive;
	msg.command = WIN_RESTORE;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	return mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
}

/**
 * hd_format_track - Format a track
 * @drive: Drive number
 * @cylinder: Cylinder number
 * @head: Head number
 * 
 * Returns 0 on success, -1 on error.
 */
int hd_format_track(unsigned int drive, unsigned int cylinder, unsigned int head)
{
	struct msg_hd_command msg;
	
	if (!(current_capability & CAP_DISK_ADMIN))
		return -1;

	msg.header.msg_id = MSG_HD_FORMAT;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);

	msg.drive = drive;
	msg.command = WIN_FORMAT;
	msg.param1 = cylinder;
	msg.param2 = head;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	return mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
}

/**
 * hd_diagnose - Run drive diagnostics
 * @drive: Drive number
 * 
 * Returns 0 if diagnostics pass, -1 on error.
 */
int hd_diagnose(unsigned int drive)
{
	struct msg_hd_command msg;
	struct msg_hd_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_DISK_ADMIN))
		return -1;

	msg.header.msg_id = MSG_HD_DIAGNOSE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.drive = drive;
	msg.command = WIN_DIAGNOSE;
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
 * hd_specify - Set drive parameters
 * @drive: Drive number
 * @params: Drive parameters (encoded as per ATA spec)
 * 
 * Returns 0 on success, -1 on error.
 */
int hd_specify(unsigned int drive, unsigned int params)
{
	struct msg_hd_command msg;
	
	if (!(current_capability & CAP_DISK_ADMIN))
		return -1;

	msg.header.msg_id = MSG_HD_SPECIFY;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);

	msg.drive = drive;
	msg.command = WIN_SPECIFY;
	msg.param1 = params;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	return mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
}

/**
 * hd_get_status - Get drive status
 * @drive: Drive number
 * @status: Output status register
 * @error: Output error register
 * 
 * Returns 0 on success, -1 on error.
 */
int hd_get_status(unsigned int drive, unsigned int *status, unsigned int *error)
{
	struct msg_hd_command msg;
	struct msg_hd_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (drive >= HD_MAX_DRIVES)
		return -1;

	msg.header.msg_id = MSG_HD_GET_STATUS;
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

	if (reply.result == 0) {
		if (status) *status = reply.data.status.status;
		if (error) *error = reply.data.status.error;
	}

	return reply.result;
}

/**
 * hd_drive_info - Drive information structure
 */
struct hd_drive_info {
	unsigned int cylinders;		/* Number of cylinders */
	unsigned int heads;		/* Number of heads */
	unsigned int sectors;		/* Sectors per track */
	unsigned int size;		/* Total size in sectors */
	unsigned int model[40];		/* Model string (if available) */
	unsigned int serial[20];	/* Serial number (if available) */
};

/**
 * hd_get_drive_info - Get drive information
 * @drive: Drive number
 * @info: Output information structure
 * 
 * Returns 0 on success, -1 on error.
 */
int hd_get_drive_info(unsigned int drive, struct hd_drive_info *info)
{
	struct msg_hd_get_info msg;
	struct msg_hd_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (drive >= HD_MAX_DRIVES || !info)
		return -1;

	msg.header.msg_id = MSG_HD_GET_INFO;
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

	if (reply.result == 0) {
		info->cylinders = reply.data.drive_info.cylinders;
		info->heads = reply.data.drive_info.heads;
		info->sectors = reply.data.drive_info.sectors;
		info->size = reply.data.drive_info.size;
	}

	return reply.result;
}

/**
 * hd_get_partitions - Read partition table
 * @drive: Drive number
 * @partitions: Output array of 4 partition structures
 * 
 * Returns 0 on success, -1 on error.
 */
int hd_get_partitions(unsigned int drive, struct partition *partitions)
{
	struct msg_hd_get_info msg;
	struct msg_hd_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (drive >= HD_MAX_DRIVES || !partitions)
		return -1;

	if (!(current_capability & CAP_DISK_INFO))
		return -1;

	msg.header.msg_id = MSG_HD_GET_PARTITIONS;
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

	if (reply.result == 0) {
		memcpy(partitions, reply.data.partitions, 
		       sizeof(struct partition) * 4);
	}

	return reply.result;
}

/*=============================================================================
 * CONVENIENCE FUNCTIONS
 *============================================================================*/

/**
 * hd_read_sector - Read a single sector
 * @drive: Drive number
 * @sector: Sector number
 * @buffer: Destination buffer (must be at least 512 bytes)
 */
static inline int hd_read_sector(unsigned int drive, unsigned int sector,
                                  void *buffer)
{
	return hd_read(drive, sector, buffer, 1);
}

/**
 * hd_write_sector - Write a single sector
 * @drive: Drive number
 * @sector: Sector number
 * @buffer: Source buffer (must be at least 512 bytes)
 */
static inline int hd_write_sector(unsigned int drive, unsigned int sector,
                                   const void *buffer)
{
	return hd_write(drive, sector, buffer, 1);
}

/**
 * hd_read_block - Read multiple contiguous sectors
 * @drive: Drive number
 * @start: Starting sector
 * @blocks: Number of blocks (1 block = 512 bytes)
 * @buffer: Destination buffer
 */
static inline int hd_read_block(unsigned int drive, unsigned int start,
                                 unsigned int blocks, void *buffer)
{
	return hd_read(drive, start, buffer, blocks);
}

/**
 * hd_write_block - Write multiple contiguous sectors
 * @drive: Drive number
 * @start: Starting sector
 * @blocks: Number of blocks
 * @buffer: Source buffer
 */
static inline int hd_write_block(unsigned int drive, unsigned int start,
                                  unsigned int blocks, const void *buffer)
{
	return hd_write(drive, start, buffer, blocks);
}

/*=============================================================================
 * CAPABILITY REQUEST FUNCTION
 *============================================================================*/

static inline int request_disk_capability(unsigned int cap)
{
	struct msg_cap_request msg;
	struct msg_cap_reply reply;
	unsigned int reply_size = sizeof(reply);

	msg.header.msg_id = MSG_CAP_REQUEST_DISK;
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

#endif /* _HDREG_H */
