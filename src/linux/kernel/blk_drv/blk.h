/*
* HISTORY
* $Log: blk.h,v $
* Revision 1.1 2026/02/15 10:15:30 pedro
* Microkernel version of block device definitions.
* Original request structures preserved for compatibility.
* All device I/O now delegated to device server via IPC.
* Added capability checking for block operations.
* [2026/02/15 pedro]
*/

/*
 * File: kernel/blk_drv/blk.h
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * Block device definitions for microkernel architecture.
 *
 * Original Linux 0.11: Direct device driver requests with elevator algorithm.
 * Microkernel version: Block I/O requests are sent as IPC messages to the
 * device server. The request queue and elevator algorithm now reside in
 * the device server. This header provides compatibility stubs.
 *
 * Security: Block device operations require CAP_DEVICE capability.
 */

#ifndef _BLK_H
#define _BLK_H

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <asm/system.h>

/*=============================================================================
 * ORIGINAL CONSTANTS (Preserved for compatibility)
 *============================================================================*/

#define NR_BLK_DEV	7

/*
 * NR_REQUEST is the number of entries in the request-queue.
 * NOTE that writes may use only the low 2/3 of these: reads
 * take precedence.
 *
 * 32 seems to be a reasonable number: enough to get some benefit
 * from the elevator-mechanism, but not so much as to lock a lot of
 * buffers when they are in the queue. 64 seems to be too many (easily
 * long pauses in reading when heavy writing/syncing is going on)
 */
#define NR_REQUEST	32

/*
 * Ok, this is an expanded form so that we can use the same
 * request for paging requests when that is implemented. In
 * paging, 'bh' is NULL, and 'waiting' is used to wait for
 * read/write completion.
 */
struct request {
	int dev;		/* -1 if no request */
	int cmd;		/* READ or WRITE */
	int errors;
	unsigned long sector;
	unsigned long nr_sectors;
	char * buffer;
	struct task_struct * waiting;
	struct buffer_head * bh;
	struct request * next;
};

/*
 * This is used in the elevator algorithm: Note that
 * reads always go before writes. This is natural: reads
 * are much more time-critical than writes.
 */
#define IN_ORDER(s1,s2) \
((s1)->cmd<(s2)->cmd || ((s1)->cmd==(s2)->cmd && \
((s1)->dev < (s2)->dev || ((s1)->dev == (s2)->dev && \
(s1)->sector < (s2)->sector))))

struct blk_dev_struct {
	void (*request_fn)(void);
	struct request * current_request;
};

extern struct blk_dev_struct blk_dev[NR_BLK_DEV];
extern struct request request[NR_REQUEST];
extern struct task_struct * wait_for_request;

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_BLK_REQUEST		0x2100	/* Block device request */
#define MSG_BLK_READ		0x2101	/* Read request */
#define MSG_BLK_WRITE		0x2102	/* Write request */
#define MSG_BLK_IOCTL		0x2103	/* Device ioctl */
#define MSG_BLK_GET_INFO	0x2104	/* Get device info */
#define MSG_BLK_REPLY		0x2105	/* Reply from device server */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_blk_request {
	struct mk_msg_header header;
	int dev;			/* Device number */
	int cmd;			/* READ or WRITE */
	unsigned long sector;		/* Starting sector */
	unsigned long nr_sectors;	/* Number of sectors */
	char *buffer;			/* Data buffer (user space) */
	unsigned long req_id;		/* Request ID for matching */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_blk_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	unsigned long req_id;		/* Request ID being replied to */
	unsigned long bytes;		/* Bytes transferred */
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * CAPABILITY FLAGS
 *============================================================================*/

#define CAP_BLK_READ		0x0010	/* Can read from block device */
#define CAP_BLK_WRITE		0x0020	/* Can write to block device */
#define CAP_BLK_ADMIN		0x0040	/* Can administer block device */

/*=============================================================================
 * MAJOR NUMBER HANDLING
 *============================================================================*/

#ifdef MAJOR_NR

/*
 * Add entries as needed. Currently the only block devices
 * supported are hard-disks and floppies.
 */

#if (MAJOR_NR == 1)
/* ram disk */
#define DEVICE_NAME "ramdisk"
#define DEVICE_REQUEST do_rd_request
#define DEVICE_NR(device) ((device) & 7)
#define DEVICE_ON(device) 
#define DEVICE_OFF(device)

#elif (MAJOR_NR == 2)
/* floppy */
#define DEVICE_NAME "floppy"
#define DEVICE_INTR do_floppy
#define DEVICE_REQUEST do_fd_request
#define DEVICE_NR(device) ((device) & 3)
#define DEVICE_ON(device) floppy_on(DEVICE_NR(device))
#define DEVICE_OFF(device) floppy_off(DEVICE_NR(device))

#elif (MAJOR_NR == 3)
/* harddisk */
#define DEVICE_NAME "harddisk"
#define DEVICE_INTR do_hd
#define DEVICE_REQUEST do_hd_request
#define DEVICE_NR(device) (MINOR(device)/5)
#define DEVICE_ON(device)
#define DEVICE_OFF(device)

#else
/* unknown blk device */
#error "unknown blk device"

#endif

#define CURRENT (blk_dev[MAJOR_NR].current_request)
#define CURRENT_DEV DEVICE_NR(CURRENT->dev)

#ifdef DEVICE_INTR
void (*DEVICE_INTR)(void) = NULL;
#endif
static void (DEVICE_REQUEST)(void);

/*=============================================================================
 * MICROKERNEL REQUEST TRACKING
 *============================================================================*/

/* Pending request tracking for IPC */
static struct pending_request {
	unsigned long req_id;
	struct request *req;
	struct task_struct *waiting;
	unsigned long timeout;
	struct pending_request *next;
} *pending_requests = NULL;

static unsigned long next_req_id = 1;

/**
 * add_pending_request - Track request sent to device server
 * @req: Original request structure
 * 
 * Returns request ID.
 */
static unsigned long add_pending_request(struct request *req)
{
	struct pending_request *p;
	unsigned long req_id;
	
	cli();
	
	p = (struct pending_request *) kmalloc(sizeof(struct pending_request));
	if (!p) {
		sti();
		return 0;
	}
	
	req_id = next_req_id++;
	p->req_id = req_id;
	p->req = req;
	p->waiting = current;
	p->timeout = jiffies + 30*HZ;  /* 30 second timeout */
	p->next = pending_requests;
	
	pending_requests = p;
	
	sti();
	return req_id;
}

/**
 * find_pending_request - Find pending request by ID
 * @req_id: Request ID
 * 
 * Returns pending request structure, or NULL.
 */
static struct pending_request *find_pending_request(unsigned long req_id)
{
	struct pending_request *p, *prev = NULL;
	
	for (p = pending_requests; p; prev = p, p = p->next) {
		if (p->req_id == req_id) {
			/* Remove from list */
			if (prev)
				prev->next = p->next;
			else
				pending_requests = p->next;
			return p;
		}
	}
	
	return NULL;
}

/**
 * check_request_timeouts - Check for timed-out requests
 */
static void check_request_timeouts(void)
{
	struct pending_request *p, *prev = NULL, *next;
	unsigned long now = jiffies;
	
	for (p = pending_requests; p; p = next) {
		next = p->next;
		
		if (p->timeout < now) {
			/* Request timed out */
			if (p->waiting)
				wake_up(&p->waiting);
			
			if (prev)
				prev->next = next;
			else
				pending_requests = next;
			
			kfree(p);
		} else {
			prev = p;
		}
	}
}

/*=============================================================================
 * BLOCK DEVICE OPERATIONS (IPC Stubs)
 *============================================================================*/

/**
 * blk_request - Send block device request to device server
 * @dev: Device number
 * @cmd: Command (READ/WRITE)
 * @sector: Starting sector
 * @nr_sectors: Number of sectors
 * @buffer: Data buffer
 * 
 * Returns 0 on success, negative error code on failure.
 */
static int blk_request(int dev, int cmd, unsigned long sector,
                        unsigned long nr_sectors, char *buffer)
{
	struct msg_blk_request msg;
	struct msg_blk_reply reply;
	unsigned int reply_size = sizeof(reply);
	unsigned long req_id;
	struct pending_request *p;
	int result;

	/* Check capability */
	if (cmd == READ && !(current_capability & CAP_BLK_READ))
		return -EPERM;
	if (cmd == WRITE && !(current_capability & CAP_BLK_WRITE))
		return -EPERM;

	/* Create pending request tracking */
	req_id = add_pending_request(NULL);  /* We don't have original req */
	if (!req_id)
		return -EAGAIN;

	/* Prepare IPC message */
	msg.header.msg_id = (cmd == READ) ? MSG_BLK_READ : MSG_BLK_WRITE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.dev = dev;
	msg.cmd = cmd;
	msg.sector = sector;
	msg.nr_sectors = nr_sectors;
	msg.buffer = buffer;
	msg.req_id = req_id;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	/* Send to device server */
	result = mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
	if (result < 0) {
		/* Failed to send */
		cli();
		find_pending_request(req_id);  /* Remove from list */
		sti();
		return -EAGAIN;
	}

	/* Wait for reply */
	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0) {
		/* Receive failed */
		cli();
		find_pending_request(req_id);  /* Remove from list */
		sti();
		return -EIO;
	}

	/* Find pending request */
	p = find_pending_request(reply.req_id);
	if (!p)
		return -EINVAL;

	/* Wake up waiting task if any */
	if (p->waiting)
		wake_up(&p->waiting);

	kfree(p);

	return reply.result;
}

/*=============================================================================
 * ORIGINAL INTERFACE COMPATIBILITY FUNCTIONS
 *============================================================================*/

static inline void unlock_buffer(struct buffer_head * bh)
{
	if (!bh->b_lock)
		printk(DEVICE_NAME ": free buffer being unlocked\n");
	bh->b_lock = 0;
	wake_up(&bh->b_wait);
}

static inline void end_request(int uptodate)
{
	DEVICE_OFF(CURRENT_DEV);
	
	if (CURRENT->bh) {
		CURRENT->bh->b_uptodate = uptodate;
		unlock_buffer(CURRENT->bh);
	}
	
	if (!uptodate) {
		printk(DEVICE_NAME " I/O error\n\r");
		printk("dev %04x, block %d\n\r", CURRENT->dev,
			CURRENT->bh->b_blocknr);
	}
	
	wake_up(&CURRENT->waiting);
	wake_up(&wait_for_request);
	
	CURRENT->dev = -1;
	CURRENT = CURRENT->next;
}

#define INIT_REQUEST \
repeat: \
	if (!CURRENT) \
		return; \
	if (MAJOR(CURRENT->dev) != MAJOR_NR) \
		panic(DEVICE_NAME ": request list destroyed"); \
	if (CURRENT->bh) { \
		if (!CURRENT->bh->b_lock) \
			panic(DEVICE_NAME ": block not locked"); \
	}

/*=============================================================================
 * REQUEST FUNCTION (IPC version)
 *============================================================================*/

/**
 * DEVICE_REQUEST - Device request handler (IPC version)
 * 
 * This function is called when there are requests in the queue.
 * Instead of directly handling I/O, it sends requests to the device server.
 */
static void DEVICE_REQUEST(void)
{
	struct request *req;
	int result;

	INIT_REQUEST;

	req = CURRENT;

	/* Check capability based on command */
	if (req->cmd == READ && !(current_capability & CAP_BLK_READ)) {
		end_request(0);
		goto repeat;
	}
	if (req->cmd == WRITE && !(current_capability & CAP_BLK_WRITE)) {
		end_request(0);
		goto repeat;
	}

	/* Send request to device server */
	result = blk_request(req->dev, req->cmd, req->sector,
	                      req->nr_sectors, req->buffer);

	if (result < 0) {
		/* I/O error */
		req->errors++;
		if (req->errors >= 3) {
			end_request(0);
		}
	} else {
		/* Success */
		end_request(1);
	}

	goto repeat;
}

/*=============================================================================
 * INTERRUPT HANDLER (Now notifies device server)
 *============================================================================*/

#ifdef DEVICE_INTR

/**
 * DEVICE_INTR - Device interrupt handler
 * 
 * Notifies device server of interrupt and wakes waiting tasks.
 */
void DEVICE_INTR(void)
{
	struct msg_blk_request msg;
	
	/* Send interrupt notification to device server */
	msg.header.msg_id = MSG_BLK_REQUEST;  /* Generic request */
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;  /* Fire and forget */
	msg.header.size = sizeof(msg);
	
	msg.dev = MAJOR_NR;
	msg.cmd = -1;  /* Special: interrupt notification */
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	
	mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
	
	/* Check for timed out requests */
	check_request_timeouts();
}

#endif /* DEVICE_INTR */

/*=============================================================================
 * INITIALIZATION
 *============================================================================*/

/**
 * blk_dev_init - Initialize block device layer
 */
void blk_dev_init(void)
{
	int i;
	
	printk("Initializing block device layer (microkernel mode)...\n");
	
	/* Initialize request array */
	for (i = 0; i < NR_REQUEST; i++) {
		request[i].dev = -1;
		request[i].next = NULL;
	}
	
	/* Initialize device structures */
	for (i = 0; i < NR_BLK_DEV; i++) {
		blk_dev[i].current_request = NULL;
		blk_dev[i].request_fn = NULL;
	}
	
	printk("Block device layer initialized.\n");
}

#endif /* MAJOR_NR */

#endif /* _BLK_H */
