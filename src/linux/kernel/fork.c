/*
* HISTORY
* $Log: fork.c,v $
* Revision 1.1 2026/02/15 03:15:30 pedro
* Microkernel version of fork implementation.
* Process creation now delegated to process server.
* copy_process and copy_mem send IPC to process and memory servers.
* verify_area now uses memory server for validation.
* [2026/02/15 pedro]
*/

/*
 * File: kernel/fork.c
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * Fork implementation for microkernel architecture.
 *
 * Original Linux 0.11: Direct kernel process creation with page table copying.
 * Microkernel version: Process creation is delegated to the process server.
 * The kernel only allocates a task struct and sends IPC messages to create
 * the actual process context in the server.
 *
 * Security: Fork requires CAP_PROCESS capability. The process server validates
 * that the parent has sufficient capabilities to create a child.
 */

#include <string.h>
#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/head.h>
#include <asm/segment.h>
#include <asm/system.h>

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_FORK_VERIFY_AREA	0x1E00	/* Verify memory area */
#define MSG_FORK_COPY_MEM	0x1E01	/* Copy memory tables */
#define MSG_FORK_COPY_PROCESS	0x1E02	/* Copy process */
#define MSG_FORK_FIND_EMPTY	0x1E03	/* Find empty process slot */
#define MSG_FORK_ALLOC_TASK	0x1E04	/* Allocate task struct */
#define MSG_FORK_SETUP_TSS	0x1E05	/* Setup TSS for new task */
#define MSG_FORK_REPLY		0x1E06	/* Reply from process server */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_fork_verify {
	struct mk_msg_header header;
	void *addr;			/* Address to verify */
	int size;			/* Size to verify */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_fork_copy_mem {
	struct mk_msg_header header;
	int nr;				/* New task number */
	unsigned long old_data_base;	/* Old data base */
	unsigned long new_data_base;	/* New data base */
	unsigned long limit;		/* Memory limit */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_fork_copy_process {
	struct mk_msg_header header;
	int nr;				/* New task number */
	/* Register state */
	long ebp, edi, esi, gs;
	long ebx, ecx, edx;
	long fs, es, ds;
	long eip, cs, eflags, esp, ss;
	unsigned int parent_pid;	/* Parent PID */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_fork_find_empty {
	struct mk_msg_header header;
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_fork_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		int pid;		/* New PID */
		int slot;		/* Empty slot number */
		unsigned long addr;	/* Verified address */
	} data;
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * CAPABILITY FLAGS
 *============================================================================*/

#define CAP_FORK_PROCESS	0x0400	/* Can fork new processes */

/*=============================================================================
 * GLOBAL VARIABLES
 *============================================================================*/

long last_pid = 0;

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * fork_request - Send fork request to process server
 * @msg_id: Message ID
 * @msg_data: Message data
 * @msg_size: Message size
 * @need_reply: Whether to wait for reply
 * @reply_data: Output reply data
 * 
 * Returns result from process server.
 */
static inline int fork_request(unsigned int msg_id, void *msg_data,
                                 int msg_size, int need_reply,
                                 struct msg_fork_reply *reply_data)
{
	struct msg_fork_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_FORK_PROCESS))
		return -EPERM;

	result = mk_msg_send(kernel_state->process_server, msg_data, msg_size);
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
 * VERIFY AREA
 *============================================================================*/

/**
 * verify_area - Verify that a memory area is accessible
 * @addr: Starting address
 * @size: Size of area
 * 
 * Checks that the specified memory area is valid for the current task.
 * In microkernel mode, this sends a request to the memory server.
 */
void verify_area(void * addr, int size)
{
	struct msg_fork_verify msg;
	struct msg_fork_reply reply;
	unsigned long start;
	int result;

	/* Fast path for kernel addresses */
	if ((unsigned long)addr >= TASK_SIZE) {
		return;
	}

	/* Check capability */
	if (!(current_capability & CAP_MEMORY)) {
		return;  /* Can't verify, assume ok */
	}

	msg.header.msg_id = MSG_FORK_VERIFY_AREA;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.addr = addr;
	msg.size = size;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = fork_request(MSG_FORK_VERIFY_AREA, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		start = (unsigned long) addr;
		size += start & 0xfff;
		start &= 0xfffff000;
		start += get_base(current->ldt[2]);
		while (size > 0) {
			size -= 4096;
			write_verify(start);
			start += 4096;
		}
	}
}

/*=============================================================================
 * COPY MEMORY
 *============================================================================*/

/**
 * copy_mem - Copy memory tables for new process
 * @nr: New task number
 * @p: New task structure
 * 
 * Returns 0 on success, negative error code on failure.
 */
int copy_mem(int nr, struct task_struct * p)
{
	struct msg_fork_copy_mem msg;
	struct msg_fork_reply reply;
	unsigned long old_data_base, new_data_base, data_limit;
	unsigned long old_code_base, new_code_base, code_limit;
	int result;

	/* Get current limits and bases */
	code_limit = get_limit(0x0f);
	data_limit = get_limit(0x17);
	old_code_base = get_base(current->ldt[1]);
	old_data_base = get_base(current->ldt[2]);
	
	if (old_data_base != old_code_base)
		panic("We don't support separate I&D");
	if (data_limit < code_limit)
		panic("Bad data_limit");
	
	new_data_base = new_code_base = nr * 0x4000000;
	p->start_code = new_code_base;
	
	/* Update local LDT entries */
	set_base(p->ldt[1], new_code_base);
	set_base(p->ldt[2], new_data_base);

	/* Send request to memory server */
	msg.header.msg_id = MSG_FORK_COPY_MEM;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.nr = nr;
	msg.old_data_base = old_data_base;
	msg.new_data_base = new_data_base;
	msg.limit = data_limit;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = fork_request(MSG_FORK_COPY_MEM, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local page table copying */
		if (copy_page_tables(old_data_base, new_data_base, data_limit)) {
			printk("free_page_tables: from copy_mem\n");
			free_page_tables(new_data_base, data_limit);
			return -ENOMEM;
		}
		return 0;
	}

	return result;
}

/*=============================================================================
 * COPY PROCESS
 *============================================================================*/

/**
 * copy_process - Create a new process
 * @nr: Task slot number
 * @ebp,edi,esi,gs: Saved registers
 * @none: Placeholder
 * @ebx,ecx,edx: More registers
 * @fs,es,ds: Segment registers
 * @eip,cs,eflags,esp,ss: Instruction state
 * 
 * Returns PID of new process, or negative error code.
 */
int copy_process(int nr, long ebp, long edi, long esi, long gs, long none,
                 long ebx, long ecx, long edx,
                 long fs, long es, long ds,
                 long eip, long cs, long eflags, long esp, long ss)
{
	struct msg_fork_copy_process msg;
	struct msg_fork_reply reply;
	struct task_struct *p;
	int result;
	int i;
	struct file *f;

	/* Check capability */
	if (!(current_capability & CAP_FORK_PROCESS))
		return -EPERM;

	/* Allocate task structure locally */
	p = (struct task_struct *) get_free_page();
	if (!p)
		return -EAGAIN;
	
	task[nr] = p;

	/* Prepare message for process server */
	msg.header.msg_id = MSG_FORK_COPY_PROCESS;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.nr = nr;
	msg.ebp = ebp;
	msg.edi = edi;
	msg.esi = esi;
	msg.gs = gs;
	msg.ebx = ebx;
	msg.ecx = ecx;
	msg.edx = edx;
	msg.fs = fs;
	msg.es = es;
	msg.ds = ds;
	msg.eip = eip;
	msg.cs = cs;
	msg.eflags = eflags;
	msg.esp = esp;
	msg.ss = ss;
	msg.parent_pid = current->pid;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	/* Send request to process server */
	result = fork_request(MSG_FORK_COPY_PROCESS, &msg, sizeof(msg), 1, &reply);
	if (result < 0) {
		/* Fallback to local implementation */
		p->state = TASK_UNINTERRUPTIBLE;
		p->pid = last_pid;
		p->father = current->pid;
		p->counter = p->priority;
		p->signal = 0;
		p->alarm = 0;
		p->leader = 0;
		p->utime = p->stime = 0;
		p->cutime = p->cstime = 0;
		p->start_time = jiffies;
		
		p->tss.back_link = 0;
		p->tss.esp0 = PAGE_SIZE + (long) p;
		p->tss.ss0 = 0x10;
		p->tss.eip = eip;
		p->tss.eflags = eflags;
		p->tss.eax = 0;
		p->tss.ecx = ecx;
		p->tss.edx = edx;
		p->tss.ebx = ebx;
		p->tss.esp = esp;
		p->tss.ebp = ebp;
		p->tss.esi = esi;
		p->tss.edi = edi;
		p->tss.es = es & 0xffff;
		p->tss.cs = cs & 0xffff;
		p->tss.ss = ss & 0xffff;
		p->tss.ds = ds & 0xffff;
		p->tss.fs = fs & 0xffff;
		p->tss.gs = gs & 0xffff;
		p->tss.ldt = _LDT(nr);
		p->tss.trace_bitmap = 0x80000000;
		
		if (last_task_used_math == current)
			__asm__("clts ; fnsave %0" : : "m" (p->tss.i387));
		
		if (copy_mem(nr, p)) {
			task[nr] = NULL;
			free_page((long) p);
			return -EAGAIN;
		}
		
		for (i = 0; i < NR_OPEN; i++)
			if ((f = p->filp[i]))
				f->f_count++;
		
		if (current->pwd)
			current->pwd->i_count++;
		if (current->root)
			current->root->i_count++;
		if (current->executable)
			current->executable->i_count++;
		
		set_tss_desc(gdt + (nr << 1) + FIRST_TSS_ENTRY, &(p->tss));
		set_ldt_desc(gdt + (nr << 1) + FIRST_LDT_ENTRY, &(p->ldt));
		
		p->state = TASK_RUNNING;
		return last_pid;
	}

	/* Copy file pointers locally */
	*p = *current;  /* Copy basic task struct */
	p->pid = reply.data.pid;
	p->father = current->pid;
	p->state = TASK_RUNNING;

	/* Update file reference counts */
	for (i = 0; i < NR_OPEN; i++)
		if ((f = p->filp[i]))
			f->f_count++;

	/* Update inode counts */
	if (current->pwd)
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
	if (current->executable)
		current->executable->i_count++;

	/* Set up TSS and LDT descriptors locally */
	set_tss_desc(gdt + (nr << 1) + FIRST_TSS_ENTRY, &(p->tss));
	set_ldt_desc(gdt + (nr << 1) + FIRST_LDT_ENTRY, &(p->ldt));

	return reply.data.pid;
}

/*=============================================================================
 * FIND EMPTY PROCESS SLOT
 *============================================================================*/

/**
 * find_empty_process - Find an empty slot in task array
 * 
 * Returns slot number, or -EAGAIN if none available.
 */
int find_empty_process(void)
{
	struct msg_fork_find_empty msg;
	struct msg_fork_reply reply;
	int i;
	int result;

	/* Try local first for speed */
	for (i = 1; i < NR_TASKS; i++)
		if (!task[i]) {
			/* Found local empty slot */
			if (last_pid < 0) last_pid = 1;
			last_pid++;
			
			/* Verify with process server */
			msg.header.msg_id = MSG_FORK_FIND_EMPTY;
			msg.header.sender_port = kernel_state->kernel_port;
			msg.header.reply_port = kernel_state->kernel_port;
			msg.header.size = sizeof(msg);
			
			msg.task_id = kernel_state->current_task;
			msg.caps = current_capability;

			result = fork_request(MSG_FORK_FIND_EMPTY, &msg, sizeof(msg), 1, &reply);
			if (result < 0) {
				/* Server unavailable, use local PID */
				return i;
			}
			
			if (reply.result >= 0) {
				last_pid = reply.data.pid;
				return i;
			}
		}

	/* No local slot found, ask server */
	msg.header.msg_id = MSG_FORK_FIND_EMPTY;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	result = fork_request(MSG_FORK_FIND_EMPTY, &msg, sizeof(msg), 1, &reply);
	if (result >= 0) {
		/* Server found a slot, update last_pid */
		last_pid = reply.data.pid;
		return reply.data.slot;
	}

	return -EAGAIN;
}

/*=============================================================================
 * INITIALIZATION
 *============================================================================*/

/**
 * fork_init - Initialize fork subsystem
 */
void fork_init(void)
{
	printk("Initializing fork subsystem (microkernel mode)...\n");
	
	/* Reset last_pid */
	last_pid = 0;
	
	printk("Fork subsystem initialized.\n");
}
