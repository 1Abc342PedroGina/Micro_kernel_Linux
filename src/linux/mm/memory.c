/*
* HISTORY
* $Log: memory.c,v $
* Revision 1.1 2026/02/15 11:15:30 pedro
* Microkernel version of memory management.
* All page operations now send IPC to memory server.
* Page tables managed by memory server.
* Copy-on-write handled by server.
* [2026/02/15 pedro]
*/

/*
 * File: mm/memory.c
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * Memory management for microkernel architecture.
 *
 * Original Linux 0.11: Direct page table manipulation, page allocation/free.
 * Microkernel version: All memory operations delegated to memory server via IPC.
 * The kernel maintains only minimal local state (mem_map cache) and forwards
 * all requests to the server.
 *
 * Security: Memory operations require CAP_MEMORY capability. The memory server
 * validates all requests and enforces protection boundaries.
 */

#include <signal.h>
#include <asm/system.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>
#include <linux/mm.h>

/*=============================================================================
 * ORIGINAL CONSTANTS (Preserved for compatibility)
 *============================================================================*/

#define LOW_MEM 0x100000
#define PAGING_MEMORY (15*1024*1024)
#define PAGING_PAGES (PAGING_MEMORY>>12)
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)
#define USED 100

#define CODE_SPACE(addr) ((((addr)+4095)&~4095) < \
current->start_code + current->end_code)

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_MEM_GET_FREE_PAGE	0x0100	/* Get free page */
#define MSG_MEM_PUT_PAGE	0x0101	/* Put page at address */
#define MSG_MEM_FREE_PAGE	0x0102	/* Free page */
#define MSG_MEM_COPY_PAGE_TABLES 0x0110	/* Copy page tables */
#define MSG_MEM_FREE_PAGE_TABLES 0x0111	/* Free page tables */
#define MSG_MEM_UN_WP_PAGE	0x0112	/* Un-write-protect page */
#define MSG_MEM_DO_WP_PAGE	0x0113	/* Handle write-protect page */
#define MSG_MEM_WRITE_VERIFY	0x0114	/* Verify write access */
#define MSG_MEM_GET_EMPTY_PAGE	0x0115	/* Get empty page */
#define MSG_MEM_TRY_TO_SHARE	0x0116	/* Try to share page */
#define MSG_MEM_SHARE_PAGE	0x0117	/* Share page */
#define MSG_MEM_DO_NO_PAGE	0x0118	/* Handle page fault */
#define MSG_MEM_INIT		0x0119	/* Initialize memory */
#define MSG_MEM_CALC		0x011A	/* Calculate memory stats */
#define MSG_MEM_REPLY		0x011B	/* Reply from memory server */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_mem_page {
	struct mk_msg_header header;
	unsigned long page;		/* Page address */
	unsigned long address;		/* Virtual address */
	unsigned long error_code;	/* Error code (for faults) */
	unsigned long from;		/* Source address */
	unsigned long to;		/* Destination address */
	long size;			/* Size */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_mem_share {
	struct mk_msg_header header;
	unsigned long address;		/* Address to share */
	struct task_struct *p;		/* Task to share with */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_mem_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		unsigned long page;	/* Page address */
		unsigned long value;	/* Generic value */
	} data;
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * GLOBAL STATE (Local cache)
 *============================================================================*/

static long HIGH_MEMORY = 0;
static unsigned char mem_map[PAGING_PAGES] = {0,};

/*=============================================================================
 * CAPABILITY FLAGS
 *============================================================================*/

#define CAP_MEM_PAGE	0x0002	/* Can allocate/free pages */

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

static inline void oom(void)
{
	printk("out of memory\n\r");
	do_exit(SIGSEGV);
}

#define invalidate() \
__asm__("movl %%eax,%%cr3"::"a" (0))

#define copy_page(from,to) \
__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024))

/**
 * mem_request - Send memory request to memory server
 * @msg_id: Message ID
 * @msg_data: Message data
 * @msg_size: Message size
 * @need_reply: Whether to wait for reply
 * @reply_data: Output reply data
 * 
 * Returns result from memory server.
 */
static inline int mem_request(unsigned int msg_id, void *msg_data,
                                int msg_size, int need_reply,
                                struct msg_mem_reply *reply_data)
{
	struct msg_mem_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	/* Check capability */
	if (!(current_capability & CAP_MEM_PAGE))
		return -EPERM;

	result = mk_msg_send(kernel_state->memory_server, msg_data, msg_size);
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
 * PAGE ALLOCATION/FREE (IPC stubs)
 *============================================================================*/

unsigned long get_free_page(void)
{
	struct msg_mem_page msg;
	struct msg_mem_reply reply;
	int result;

	/* Check capability */
	if (!(current_capability & CAP_MEM_PAGE)) {
		/* Try local fallback */
		register unsigned long __res asm("ax");
		__asm__("std ; repne ; scasb\n\t"
			"jne 1f\n\t"
			"movb $1,1(%%edi)\n\t"
			"sall $12,%%ecx\n\t"
			"addl %2,%%ecx\n\t"
			"movl %%ecx,%%edx\n\t"
			"movl $1024,%%ecx\n\t"
			"leal 4092(%%edx),%%edi\n\t"
			"rep ; stosl\n\t"
			" movl %%edx,%%eax\n"
			"1: cld"
			:"=a" (__res)
			:"0" (0),"i" (LOW_MEM),"c" (PAGING_PAGES),
			 "D" (mem_map+PAGING_PAGES-1)
			);
		return __res;
	}

	msg.header.msg_id = MSG_MEM_GET_FREE_PAGE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.page = 0;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mem_request(MSG_MEM_GET_FREE_PAGE, &msg, sizeof(msg), 1, &reply);
	if (result < 0)
		return 0;

	return reply.data.page;
}

void free_page(unsigned long addr)
{
	struct msg_mem_page msg;

	if (addr < LOW_MEM) return;
	if (addr >= HIGH_MEMORY) {
		printk("trying to free nonexistent page\n");
		return;
	}

	/* Update local cache */
	addr -= LOW_MEM;
	addr >>= 12;
	if (mem_map[addr] > 0) {
		mem_map[addr]--;
	}

	/* Send to memory server if we have capability */
	if (current_capability & CAP_MEM_PAGE) {
		msg.header.msg_id = MSG_MEM_FREE_PAGE;
		msg.header.sender_port = kernel_state->kernel_port;
		msg.header.reply_port = 0;
		msg.header.size = sizeof(msg);
		
		msg.page = addr << 12;
		msg.task_id = kernel_state->current_task;
		msg.caps = current_capability;

		mem_request(MSG_MEM_FREE_PAGE, &msg, sizeof(msg), 0, NULL);
	}
}

/*=============================================================================
 * PAGE TABLE OPERATIONS (IPC stubs)
 *============================================================================*/

int free_page_tables(unsigned long from, unsigned long size)
{
	struct msg_mem_page msg;
	struct msg_mem_reply reply;
	int result;

	if (from & 0x3fffff)
		panic("free_page_tables called with wrong alignment");
	if (!from)
		panic("Trying to free up swapper memory space");

	msg.header.msg_id = MSG_MEM_FREE_PAGE_TABLES;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.from = from;
	msg.size = size;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mem_request(MSG_MEM_FREE_PAGE_TABLES, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		unsigned long *pg_table;
		unsigned long *dir, nr;

		size = (size + 0x3fffff) >> 22;
		dir = (unsigned long *) ((from>>20) & 0xffc);
		for ( ; size-->0 ; dir++) {
			if (!(1 & *dir))
				continue;
			pg_table = (unsigned long *) (0xfffff000 & *dir);
			for (nr=0 ; nr<1024 ; nr++) {
				if (1 & *pg_table)
					free_page(0xfffff000 & *pg_table);
				*pg_table = 0;
				pg_table++;
			}
			free_page(0xfffff000 & *dir);
			*dir = 0;
		}
		invalidate();
		return 0;
	}

	return result;
}

int copy_page_tables(unsigned long from, unsigned long to, long size)
{
	struct msg_mem_page msg;
	struct msg_mem_reply reply;
	int result;

	if ((from&0x3fffff) || (to&0x3fffff))
		panic("copy_page_tables called with wrong alignment");

	msg.header.msg_id = MSG_MEM_COPY_PAGE_TABLES;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.from = from;
	msg.to = to;
	msg.size = size;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mem_request(MSG_MEM_COPY_PAGE_TABLES, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		unsigned long * from_page_table;
		unsigned long * to_page_table;
		unsigned long this_page;
		unsigned long * from_dir, * to_dir;
		unsigned long nr;

		from_dir = (unsigned long *) ((from>>20) & 0xffc);
		to_dir = (unsigned long *) ((to>>20) & 0xffc);
		size = ((unsigned) (size+0x3fffff)) >> 22;
		for( ; size-->0 ; from_dir++,to_dir++) {
			if (1 & *to_dir)
				panic("copy_page_tables: already exist");
			if (!(1 & *from_dir))
				continue;
			from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
			if (!(to_page_table = (unsigned long *) get_free_page()))
				return -1;
			*to_dir = ((unsigned long) to_page_table) | 7;
			nr = (from==0)?0xA0:1024;
			for ( ; nr-- > 0 ; from_page_table++,to_page_table++) {
				this_page = *from_page_table;
				if (!(1 & this_page))
					continue;
				this_page &= ~2;
				*to_page_table = this_page;
				if (this_page > LOW_MEM) {
					*from_page_table = this_page;
					this_page -= LOW_MEM;
					this_page >>= 12;
					mem_map[this_page]++;
				}
			}
		}
		invalidate();
		return 0;
	}

	return result;
}

/*=============================================================================
 * PAGE MAPPING (IPC stub)
 *============================================================================*/

unsigned long put_page(unsigned long page, unsigned long address)
{
	struct msg_mem_page msg;
	struct msg_mem_reply reply;
	int result;

	if (page < LOW_MEM || page >= HIGH_MEMORY)
		printk("Trying to put page %p at %p\n", page, address);
	
	if (mem_map[(page-LOW_MEM)>>12] != 1)
		printk("mem_map disagrees with %p at %p\n", page, address);

	msg.header.msg_id = MSG_MEM_PUT_PAGE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.page = page;
	msg.address = address;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mem_request(MSG_MEM_PUT_PAGE, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		unsigned long tmp, *page_table;

		page_table = (unsigned long *) ((address>>20) & 0xffc);
		if ((*page_table)&1)
			page_table = (unsigned long *) (0xfffff000 & *page_table);
		else {
			if (!(tmp=get_free_page()))
				return 0;
			*page_table = tmp|7;
			page_table = (unsigned long *) tmp;
		}
		page_table[(address>>12) & 0x3ff] = page | 7;
		return page;
	}

	return reply.data.page;
}

/*=============================================================================
 * COPY-ON-WRITE HANDLING (IPC stubs)
 *============================================================================*/

void un_wp_page(unsigned long * table_entry)
{
	struct msg_mem_page msg;
	struct msg_mem_reply reply;
	unsigned long old_page;
	int result;

	old_page = 0xfffff000 & *table_entry;

	msg.header.msg_id = MSG_MEM_UN_WP_PAGE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.page = old_page;
	msg.address = (unsigned long)table_entry;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mem_request(MSG_MEM_UN_WP_PAGE, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		unsigned long new_page;

		if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)]==1) {
			*table_entry |= 2;
			invalidate();
			return;
		}
		if (!(new_page=get_free_page()))
			oom();
		if (old_page >= LOW_MEM)
			mem_map[MAP_NR(old_page)]--;
		*table_entry = new_page | 7;
		invalidate();
		copy_page(old_page, new_page);
	}
}

void do_wp_page(unsigned long error_code, unsigned long address)
{
	struct msg_mem_page msg;

#if 0
	if (CODE_SPACE(address))
		do_exit(SIGSEGV);
#endif

	msg.header.msg_id = MSG_MEM_DO_WP_PAGE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.error_code = error_code;
	msg.address = address;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mem_request(MSG_MEM_DO_WP_PAGE, &msg, sizeof(msg), 0, NULL);
}

void write_verify(unsigned long address)
{
	struct msg_mem_page msg;

	msg.header.msg_id = MSG_MEM_WRITE_VERIFY;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.address = address;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mem_request(MSG_MEM_WRITE_VERIFY, &msg, sizeof(msg), 0, NULL);
}

void get_empty_page(unsigned long address)
{
	struct msg_mem_page msg;
	struct msg_mem_reply reply;
	int result;

	msg.header.msg_id = MSG_MEM_GET_EMPTY_PAGE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.address = address;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mem_request(MSG_MEM_GET_EMPTY_PAGE, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		unsigned long tmp;
		if (!(tmp=get_free_page()) || !put_page(tmp,address)) {
			free_page(tmp);
			oom();
		}
	}
}

/*=============================================================================
 * PAGE SHARING (IPC stubs)
 *============================================================================*/

static int try_to_share(unsigned long address, struct task_struct * p)
{
	struct msg_mem_share msg;
	struct msg_mem_reply reply;
	int result;

	msg.header.msg_id = MSG_MEM_TRY_TO_SHARE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.address = address;
	msg.p = p;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mem_request(MSG_MEM_TRY_TO_SHARE, &msg, sizeof(msg), 1, &reply);
	if (result < 0)
		return 0;

	return result;
}

static int share_page(unsigned long address)
{
	struct msg_mem_share msg;
	struct msg_mem_reply reply;
	int result;

	if (!current->executable)
		return 0;
	if (current->executable->i_count < 2)
		return 0;

	msg.header.msg_id = MSG_MEM_SHARE_PAGE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.address = address;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = mem_request(MSG_MEM_SHARE_PAGE, &msg, sizeof(msg), 1, &reply);
	if (result < 0)
		return 0;

	return result;
}

/*=============================================================================
 * PAGE FAULT HANDLING (IPC stub)
 *============================================================================*/

void do_no_page(unsigned long error_code, unsigned long address)
{
	struct msg_mem_page msg;
	int nr[4];
	unsigned long tmp;
	unsigned long page;
	int block, i;

	address &= 0xfffff000;
	tmp = address - current->start_code;
	
	if (!current->executable || tmp >= current->end_data) {
		get_empty_page(address);
		return;
	}
	
	if (share_page(tmp))
		return;

	/* Handle locally for now - would need complex IPC */
	if (!(page = get_free_page()))
		oom();
	
	block = 1 + tmp/BLOCK_SIZE;
	for (i=0 ; i<4 ; block++,i++)
		nr[i] = bmap(current->executable, block);
	
	bread_page(page, current->executable->i_dev, nr);
	
	i = tmp + 4096 - current->end_data;
	tmp = page + 4096;
	while (i-- > 0) {
		tmp--;
		*(char *)tmp = 0;
	}
	
	if (put_page(page, address))
		return;
	
	free_page(page);
	oom();
}

/*=============================================================================
 * MEMORY INITIALIZATION
 *============================================================================*/

void mem_init(long start_mem, long end_mem)
{
	struct msg_mem_page msg;
	int i;

	HIGH_MEMORY = end_mem;
	
	/* Initialize local mem_map */
	for (i=0 ; i<PAGING_PAGES ; i++)
		mem_map[i] = USED;
	
	i = MAP_NR(start_mem);
	end_mem -= start_mem;
	end_mem >>= 12;
	while (end_mem-- > 0)
		mem_map[i++] = 0;

	/* Notify memory server */
	msg.header.msg_id = MSG_MEM_INIT;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.from = start_mem;
	msg.to = end_mem;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mem_request(MSG_MEM_INIT, &msg, sizeof(msg), 0, NULL);
}

/*=============================================================================
 * MEMORY STATISTICS
 *============================================================================*/

void calc_mem(void)
{
	struct msg_mem_page msg;
	int i, j, k, free = 0;
	long * pg_tbl;

	/* Local calculation */
	for (i = 0; i < PAGING_PAGES; i++)
		if (!mem_map[i]) free++;
	printk("%d pages free (of %d)\n\r", free, PAGING_PAGES);
	
	for (i = 2; i < 1024; i++) {
		if (1 & pg_dir[i]) {
			pg_tbl = (long *) (0xfffff000 & pg_dir[i]);
			for (j = k = 0; j < 1024; j++)
				if (pg_tbl[j] & 1)
					k++;
			printk("Pg-dir[%d] uses %d pages\n", i, k);
		}
	}

	/* Request server statistics */
	msg.header.msg_id = MSG_MEM_CALC;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mem_request(MSG_MEM_CALC, &msg, sizeof(msg), 0, NULL);
}
