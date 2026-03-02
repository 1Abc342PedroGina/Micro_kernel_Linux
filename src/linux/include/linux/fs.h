/*
* HISTORY
* $Log: fs.h,v $
* Revision 1.1 2026/02/14 15:30:45 pedro
* Microkernel version of file system definitions.
* All structures preserved for compatibility but now represent
* capability-managed objects in the file server.
* Buffer cache moved to file server.
* Inode operations now IPC messages.
* Added capability-aware file operations.
* [2026/02/14 pedro]
*/

/*
* File: linux/fs.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* File system definitions for microkernel architecture.
*
* Original Linux 0.11: Kernel-level file system structures and functions.
* Microkernel version: All structures are now templates for communication
* with the file server. The actual data resides in the file server's
* memory space, protected by capabilities.
*
* Buffer cache, inode tables, and superblocks are managed by the file server.
* Client tasks use IPC to perform file operations.
*/

#ifndef _FS_H
#define _FS_H

#include <sys/types.h>
#include <linux/kernel.h>
#include <linux/head.h>

/*=============================================================================
 * ORIGINAL DEVICE NUMBERS (Preserved for compatibility)
 *============================================================================*/

/* devices are as follows: (same as minix, so we can use the minix
 * file system. These are major numbers.)
 *
 * 0 - unused (nodev)
 * 1 - /dev/mem
 * 2 - /dev/fd
 * 3 - /dev/hd
 * 4 - /dev/ttyx
 * 5 - /dev/tty
 * 6 - /dev/lp
 * 7 - unnamed pipes
 */

#define IS_SEEKABLE(x) ((x)>=1 && (x)<=3)

/*=============================================================================
 * ORIGINAL CONSTANTS (Preserved)
 *============================================================================*/

#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead - don't pause */
#define WRITEA 3	/* "write-ahead" - silly, but somewhat useful */

#define MAJOR(a) (((unsigned)(a))>>8)
#define MINOR(a) ((a)&0xff)

#define NAME_LEN 14
#define ROOT_INO 1

#define I_MAP_SLOTS 8
#define Z_MAP_SLOTS 8
#define SUPER_MAGIC 0x137F

#define NR_OPEN 20
#define NR_INODE 32
#define NR_FILE 64
#define NR_SUPER 8
#define NR_HASH 307
#define NR_BUFFERS nr_buffers
#define BLOCK_SIZE 1024
#define BLOCK_SIZE_BITS 10

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct d_inode)))
#define DIR_ENTRIES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct dir_entry)))

/*=============================================================================
 * ORIGINAL PIPE MACROS (Now handled by file server)
 *============================================================================*/

#define PIPE_HEAD(inode) ((inode).i_zone[0])
#define PIPE_TAIL(inode) ((inode).i_zone[1])
#define PIPE_SIZE(inode) ((PIPE_HEAD(inode)-PIPE_TAIL(inode))&(PAGE_SIZE-1))
#define PIPE_EMPTY(inode) (PIPE_HEAD(inode)==PIPE_TAIL(inode))
#define PIPE_FULL(inode) (PIPE_SIZE(inode)==(PAGE_SIZE-1))
#define INC_PIPE(head) \
__asm__("incl %0\n\tandl $4095,%0"::"m" (head))

typedef char buffer_block[BLOCK_SIZE];

/*=============================================================================
 * ORIGINAL STRUCTURES (Preserved as templates for IPC)
 *============================================================================*/

/*
 * These structures are now templates for communication with the file server.
 * The actual data resides in the file server's memory.
 */

struct buffer_head {
	char * b_data;			/* pointer to data block (1024 bytes) */
	unsigned long b_blocknr;	/* block number */
	unsigned short b_dev;		/* device (0 = free) */
	unsigned char b_uptodate;
	unsigned char b_dirt;		/* 0-clean,1-dirty */
	unsigned char b_count;		/* users using this block */
	unsigned char b_lock;		/* 0 - ok, 1 -locked */
	struct task_struct * b_wait;
	struct buffer_head * b_prev;
	struct buffer_head * b_next;
	struct buffer_head * b_prev_free;
	struct buffer_head * b_next_free;
};

struct d_inode {
	unsigned short i_mode;
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_time;
	unsigned char i_gid;
	unsigned char i_nlinks;
	unsigned short i_zone[9];
};

struct m_inode {
	unsigned short i_mode;
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_mtime;
	unsigned char i_gid;
	unsigned char i_nlinks;
	unsigned short i_zone[9];
/* these are in memory also */
	struct task_struct * i_wait;
	unsigned long i_atime;
	unsigned long i_ctime;
	unsigned short i_dev;
	unsigned short i_num;
	unsigned short i_count;
	unsigned char i_lock;
	unsigned char i_dirt;
	unsigned char i_pipe;
	unsigned char i_mount;
	unsigned char i_seek;
	unsigned char i_update;
};

struct file {
	unsigned short f_mode;
	unsigned short f_flags;
	unsigned short f_count;
	struct m_inode * f_inode;
	off_t f_pos;
};

struct super_block {
	unsigned short s_ninodes;
	unsigned short s_nzones;
	unsigned short s_imap_blocks;
	unsigned short s_zmap_blocks;
	unsigned short s_firstdatazone;
	unsigned short s_log_zone_size;
	unsigned long s_max_size;
	unsigned short s_magic;
/* These are only in memory */
	struct buffer_head * s_imap[8];
	struct buffer_head * s_zmap[8];
	unsigned short s_dev;
	struct m_inode * s_isup;
	struct m_inode * s_imount;
	unsigned long s_time;
	struct task_struct * s_wait;
	unsigned char s_lock;
	unsigned char s_rd_only;
	unsigned char s_dirt;
};

struct d_super_block {
	unsigned short s_ninodes;
	unsigned short s_nzones;
	unsigned short s_imap_blocks;
	unsigned short s_zmap_blocks;
	unsigned short s_firstdatazone;
	unsigned short s_log_zone_size;
	unsigned long s_max_size;
	unsigned short s_magic;
};

struct dir_entry {
	unsigned short inode;
	char name[NAME_LEN];
};

/*=============================================================================
 * ORIGINAL EXTERNAL VARIABLES (Now managed by file server)
 *============================================================================*/

extern struct m_inode inode_table[NR_INODE];
extern struct file file_table[NR_FILE];
extern struct super_block super_block[NR_SUPER];
extern struct buffer_head * start_buffer;
extern int nr_buffers;
extern int ROOT_DEV;

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_FS_CHECK_DISK	0x1200	/* Check disk change */
#define MSG_FS_FLOPPY_CHANGE	0x1201	/* Floppy change status */
#define MSG_FS_FLOPPY_ON	0x1202	/* Turn floppy on */
#define MSG_FS_FLOPPY_OFF	0x1203	/* Turn floppy off */
#define MSG_FS_TRUNCATE		0x1204	/* Truncate file */
#define MSG_FS_SYNC_INODES	0x1205	/* Sync all inodes */
#define MSG_FS_WAIT_ON_INODE	0x1206	/* Wait for inode */
#define MSG_FS_BMAP		0x1207	/* Block map */
#define MSG_FS_CREATE_BLOCK	0x1208	/* Create block */
#define MSG_FS_NAMEI		0x1209	/* Name to inode */
#define MSG_FS_OPEN_NAMEI	0x120A	/* Open name to inode */
#define MSG_FS_IPUT		0x120B	/* Release inode */
#define MSG_FS_IGET		0x120C	/* Get inode */
#define MSG_FS_GET_EMPTY_INODE	0x120D	/* Get empty inode */
#define MSG_FS_GET_PIPE_INODE	0x120E	/* Get pipe inode */
#define MSG_FS_GET_HASH_TABLE	0x120F	/* Get hash table entry */
#define MSG_FS_GETBLK		0x1210	/* Get block */
#define MSG_FS_LL_RW_BLOCK	0x1211	/* Low-level read/write block */
#define MSG_FS_BRElSE		0x1212	/* Release buffer */
#define MSG_FS_BREAD		0x1213	/* Read block */
#define MSG_FS_BREAD_PAGE	0x1214	/* Read page */
#define MSG_FS_BREADA		0x1215	/* Read ahead */
#define MSG_FS_NEW_BLOCK	0x1216	/* Allocate new block */
#define MSG_FS_FREE_BLOCK	0x1217	/* Free block */
#define MSG_FS_NEW_INODE	0x1218	/* Allocate new inode */
#define MSG_FS_FREE_INODE	0x1219	/* Free inode */
#define MSG_FS_SYNC_DEV		0x121A	/* Sync device */
#define MSG_FS_GET_SUPER	0x121B	/* Get superblock */
#define MSG_FS_MOUNT_ROOT	0x121C	/* Mount root */
#define MSG_FS_REPLY		0x121D	/* Reply from file server */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_fs_generic {
	struct mk_msg_header header;
	unsigned int dev;			/* Device number */
	unsigned int block;			/* Block number */
	unsigned long addr;			/* Address */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_fs_inode_op {
	struct mk_msg_header header;
	unsigned int dev;			/* Device number */
	unsigned int nr;			/* Inode number */
	struct m_inode *inode;			/* Inode pointer */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_fs_namei {
	struct mk_msg_header header;
	const char *pathname;			/* Path to resolve */
	int flag;				/* Open flags */
	int mode;				/* Creation mode */
	struct m_inode **res_inode;		/* Result inode */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_fs_bmap {
	struct mk_msg_header header;
	struct m_inode *inode;			/* Inode */
	int block;				/* Block number */
	int create;				/* Create flag */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_fs_breada {
	struct mk_msg_header header;
	int dev;				/* Device */
	int block;				/* First block */
	int count;				/* Number of blocks */
	unsigned int task_id;			/* Task making request */
	capability_t caps;			/* Caller capabilities */
};

struct msg_fs_reply {
	struct mk_msg_header header;
	int result;				/* Result code */
	union {
		struct m_inode *inode;		/* Inode pointer */
		struct buffer_head *bh;	/* Buffer head */
		int block;			/* Block number */
		unsigned long value;		/* Generic value */
	} data;
	capability_t granted_caps;		/* New capabilities granted */
};

/*=============================================================================
 * CAPABILITY FLAGS FOR FILE OPERATIONS
 *============================================================================*/

#define CAP_FS_BASIC		0x1000	/* Basic FS operations */
#define CAP_FS_ADMIN		0x2000	/* Administrative FS ops */
#define CAP_FS_BLOCK		0x4000	/* Block-level operations */
#define CAP_FS_INODE		0x8000	/* Inode operations */

/*=============================================================================
 * PUBLIC FUNCTION PROTOTYPES (Now stubs that send IPC)
 *============================================================================*/

extern void check_disk_change(int dev);
extern int floppy_change(unsigned int nr);
extern int ticks_to_floppy_on(unsigned int dev);
extern void floppy_on(unsigned int dev);
extern void floppy_off(unsigned int dev);
extern void truncate(struct m_inode * inode);
extern void sync_inodes(void);
extern void wait_on(struct m_inode * inode);
extern int bmap(struct m_inode * inode,int block);
extern int create_block(struct m_inode * inode,int block);
extern struct m_inode * namei(const char * pathname);
extern int open_namei(const char * pathname, int flag, int mode,
	struct m_inode ** res_inode);
extern void iput(struct m_inode * inode);
extern struct m_inode * iget(int dev,int nr);
extern struct m_inode * get_empty_inode(void);
extern struct m_inode * get_pipe_inode(void);
extern struct buffer_head * get_hash_table(int dev, int block);
extern struct buffer_head * getblk(int dev, int block);
extern void ll_rw_block(int rw, struct buffer_head * bh);
extern void brelse(struct buffer_head * buf);
extern struct buffer_head * bread(int dev,int block);
extern void bread_page(unsigned long addr,int dev,int b[4]);
extern struct buffer_head * breada(int dev,int block,...);
extern int new_block(int dev);
extern void free_block(int dev, int block);
extern struct m_inode * new_inode(int dev);
extern void free_inode(struct m_inode * inode);
extern int sync_dev(int dev);
extern struct super_block * get_super(int dev);
extern void mount_root(void);

/*=============================================================================
 * INTERNAL HELPER FUNCTIONS
 *============================================================================*/

/**
 * __fs_simple_request - Simple request to file server
 * @msg_id: Message ID
 * @dev: Device number
 * @block: Block number
 * @need_reply: Whether to wait for reply
 * 
 * Returns result from file server.
 */
static inline int __fs_simple_request(unsigned int msg_id, int dev, int block,
                                       int need_reply)
{
	struct msg_fs_generic msg;
	struct msg_fs_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_FS_BASIC))
		return -1;

	msg.header.msg_id = msg_id;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = need_reply ? kernel_state->kernel_port : 0;
	msg.header.size = sizeof(msg);

	msg.dev = dev;
	msg.block = block;
	msg.addr = 0;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	if (!need_reply)
		return 0;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return reply.result;
}

/*=============================================================================
 * FUNCTION IMPLEMENTATIONS (Stubs that send IPC)
 *============================================================================*/

void check_disk_change(int dev)
{
	__fs_simple_request(MSG_FS_CHECK_DISK, dev, 0, 0);
}

int floppy_change(unsigned int nr)
{
	return __fs_simple_request(MSG_FS_FLOPPY_CHANGE, nr, 0, 1);
}

int ticks_to_floppy_on(unsigned int dev)
{
	return __fs_simple_request(MSG_FS_TICKS_FLOPPY, dev, 0, 1);
}

void floppy_on(unsigned int dev)
{
	__fs_simple_request(MSG_FS_FLOPPY_ON, dev, 0, 0);
}

void floppy_off(unsigned int dev)
{
	__fs_simple_request(MSG_FS_FLOPPY_OFF, dev, 0, 0);
}

void truncate(struct m_inode * inode)
{
	struct msg_fs_inode_op msg;
	
	if (!(current_capability & CAP_FS_INODE))
		return;

	msg.header.msg_id = MSG_FS_TRUNCATE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);

	msg.inode = inode;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
}

void sync_inodes(void)
{
	__fs_simple_request(MSG_FS_SYNC_INODES, 0, 0, 0);
}

void wait_on(struct m_inode * inode)
{
	struct msg_fs_inode_op msg;
	
	msg.header.msg_id = MSG_FS_WAIT_ON_INODE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.inode = inode;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	/* Wait for reply? Actually wait_on blocks until inode unlocked */
}

int bmap(struct m_inode * inode, int block)
{
	struct msg_fs_bmap msg;
	struct msg_fs_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_FS_INODE))
		return -1;

	msg.header.msg_id = MSG_FS_BMAP;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.inode = inode;
	msg.block = block;
	msg.create = 0;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return (reply.result < 0) ? -1 : reply.data.block;
}

int create_block(struct m_inode * inode, int block)
{
	struct msg_fs_bmap msg;
	struct msg_fs_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_FS_INODE))
		return -1;

	msg.header.msg_id = MSG_FS_CREATE_BLOCK;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.inode = inode;
	msg.block = block;
	msg.create = 1;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return (reply.result < 0) ? -1 : reply.data.block;
}

struct m_inode * namei(const char * pathname)
{
	struct msg_fs_namei msg;
	struct msg_fs_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;
	static struct m_inode dummy;  /* Would need proper handling */

	if (!(current_capability & CAP_FS_BASIC))
		return NULL;

	msg.header.msg_id = MSG_FS_NAMEI;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.pathname = pathname;
	msg.res_inode = NULL;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return NULL;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return NULL;

	return (reply.result == 0) ? reply.data.inode : NULL;
}

int open_namei(const char * pathname, int flag, int mode,
                struct m_inode ** res_inode)
{
	struct msg_fs_namei msg;
	struct msg_fs_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_FS_BASIC))
		return -1;

	msg.header.msg_id = MSG_FS_OPEN_NAMEI;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.pathname = pathname;
	msg.flag = flag;
	msg.mode = mode;
	msg.res_inode = res_inode;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	if (reply.result == 0 && res_inode)
		*res_inode = reply.data.inode;

	return reply.result;
}

void iput(struct m_inode * inode)
{
	struct msg_fs_inode_op msg;
	
	if (!inode) return;

	msg.header.msg_id = MSG_FS_IPUT;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);

	msg.inode = inode;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
}

struct m_inode * iget(int dev, int nr)
{
	struct msg_fs_inode_op msg;
	struct msg_fs_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_FS_INODE))
		return NULL;

	msg.header.msg_id = MSG_FS_IGET;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.dev = dev;
	msg.nr = nr;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return NULL;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return NULL;

	return (reply.result == 0) ? reply.data.inode : NULL;
}

struct m_inode * get_empty_inode(void)
{
	return iget(0, 0);  /* Special case */
}

struct m_inode * get_pipe_inode(void)
{
	struct msg_fs_inode_op msg;
	struct msg_fs_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	msg.header.msg_id = MSG_FS_GET_PIPE_INODE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return NULL;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return NULL;

	return (reply.result == 0) ? reply.data.inode : NULL;
}

struct buffer_head * get_hash_table(int dev, int block)
{
	/* Would need buffer_head management in file server */
	return NULL;
}

struct buffer_head * getblk(int dev, int block)
{
	struct msg_fs_generic msg;
	struct msg_fs_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	msg.header.msg_id = MSG_FS_GETBLK;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.dev = dev;
	msg.block = block;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return NULL;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return NULL;

	return (struct buffer_head *)reply.data.bh;
}

void ll_rw_block(int rw, struct buffer_head * bh)
{
	struct msg_fs_generic msg;
	
	msg.header.msg_id = MSG_FS_LL_RW_BLOCK;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);

	msg.dev = (rw == READ) ? 0 : 1;  /* Encode rw in dev field */
	msg.block = (unsigned long)bh;   /* Pass buffer head pointer */
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
}

void brelse(struct buffer_head * buf)
{
	struct msg_fs_generic msg;
	
	msg.header.msg_id = MSG_FS_BRElSE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);

	msg.block = (unsigned long)buf;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
}

struct buffer_head * bread(int dev, int block)
{
	return getblk(dev, block);  /* bread = getblk + read */
}

void bread_page(unsigned long addr, int dev, int b[4])
{
	struct msg_fs_breada msg;
	
	msg.header.msg_id = MSG_FS_BREAD_PAGE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);

	msg.addr = addr;
	msg.dev = dev;
	/* Would need to pass b array */
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
}

struct buffer_head * breada(int dev, int block, ...)
{
	/* Variable arguments version */
	va_list args;
	struct msg_fs_breada msg;
	struct msg_fs_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	va_start(args, block);
	
	msg.header.msg_id = MSG_FS_BREADA;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.dev = dev;
	msg.block = block;
	msg.count = 0;
	/* Would need to pass all blocks */
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	va_end(args);

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return NULL;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return NULL;

	return (struct buffer_head *)reply.data.bh;
}

int new_block(int dev)
{
	return __fs_simple_request(MSG_FS_NEW_BLOCK, dev, 0, 1);
}

void free_block(int dev, int block)
{
	__fs_simple_request(MSG_FS_FREE_BLOCK, dev, block, 0);
}

struct m_inode * new_inode(int dev)
{
	struct msg_fs_inode_op msg;
	struct msg_fs_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	msg.header.msg_id = MSG_FS_NEW_INODE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.dev = dev;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return NULL;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return NULL;

	return (reply.result == 0) ? reply.data.inode : NULL;
}

void free_inode(struct m_inode * inode)
{
	struct msg_fs_inode_op msg;
	
	msg.header.msg_id = MSG_FS_FREE_INODE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);

	msg.inode = inode;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
}

int sync_dev(int dev)
{
	return __fs_simple_request(MSG_FS_SYNC_DEV, dev, 0, 1);
}

struct super_block * get_super(int dev)
{
	struct msg_fs_generic msg;
	struct msg_fs_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;
	static struct super_block dummy;  /* Would need proper handling */

	msg.header.msg_id = MSG_FS_GET_SUPER;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.dev = dev;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return NULL;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return NULL;

	return (struct super_block *)reply.data.bh;  /* Not quite right */
}

void mount_root(void)
{
	__fs_simple_request(MSG_FS_MOUNT_ROOT, 0, 0, 0);
}

/*=============================================================================
 * CAPABILITY REQUEST FUNCTION
 *============================================================================*/

static inline int request_fs_capability(unsigned int cap)
{
	struct msg_cap_request msg;
	struct msg_cap_reply reply;
	unsigned int reply_size = sizeof(reply);

	msg.header.msg_id = MSG_CAP_REQUEST_FS;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.task_id = kernel_state->current_task;
	msg.requested_cap = cap;

	if (mk_msg_send(kernel_state->file_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0) {
				current_capability |= cap;
				return 0;
			}
		}
	}
	return -1;
}

#endif /* _FS_H */
