/*
* HISTORY
* $Log: sched.h,v $
* Revision 1.1 2026/02/14 19:30:45 pedro
* Microkernel version of scheduler and task definitions.
* Original structures preserved for compatibility.
* TSS/LDT now represent capability contexts.
* switch_to macro now sends IPC to process server.
* Added capability-aware task fields.
* [2026/02/14 pedro]
*/

/*
* File: linux/sched.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* Scheduler and task definitions for microkernel architecture.
*
* Original Linux 0.11: Direct hardware task switching using TSS/LDT.
* Microkernel version: Tasks are managed by the process server.
* TSS/LDT structures are now templates for capability contexts.
* Context switching is done via IPC to the process server.
*
* Security: Each task has associated capabilities that control
* what resources it can access. The process server validates
* all task operations against these capabilities.
*/

#ifndef _SCHED_H
#define _SCHED_H

#include <sys/types.h>
#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <signal.h>

/*=============================================================================
 * ORIGINAL CONSTANTS (Preserved for compatibility)
 *============================================================================*/

#define NR_TASKS 64
#define HZ 100

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS-1]

#if (NR_OPEN > 32)
#error "Currently the close-on-exec-flags are in one word, max 32 files/proc"
#endif

/* Task states (preserved) */
#define TASK_RUNNING		0
#define TASK_INTERRUPTIBLE	1
#define TASK_UNINTERRUPTIBLE	2
#define TASK_ZOMBIE		3
#define TASK_STOPPED		4

/* Microkernel task state extensions */
#define TASK_SUSPENDED		5	/* Suspended waiting for IPC */
#define TASK_WAITING_IPC	6	/* Waiting for IPC reply */
#define TASK_WAITING_CAP	7	/* Waiting for capability */

#ifndef NULL
#define NULL ((void *) 0)
#endif

/*=============================================================================
 * ORIGINAL STRUCTURES (Preserved as templates)
 *============================================================================*/

struct i387_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
};

/*
 * tss_struct - Task State Segment
 * 
 * Original: Hardware TSS for x86 task switching.
 * Microkernel: Now represents a task's capability context.
 * The fields are used by the process server to manage
 * register state across IPC boundaries.
 */
struct tss_struct {
	long	back_link;	/* 16 high bits zero */
	long	esp0;
	long	ss0;		/* 16 high bits zero */
	long	esp1;
	long	ss1;		/* 16 high bits zero */
	long	esp2;
	long	ss2;		/* 16 high bits zero */
	long	cr3;
	long	eip;
	long	eflags;
	long	eax,ecx,edx,ebx;
	long	esp;
	long	ebp;
	long	esi;
	long	edi;
	long	es;		/* 16 high bits zero */
	long	cs;		/* 16 high bits zero */
	long	ss;		/* 16 high bits zero */
	long	ds;		/* 16 high bits zero */
	long	fs;		/* 16 high bits zero */
	long	gs;		/* 16 high bits zero */
	long	ldt;		/* 16 high bits zero */
	long	trace_bitmap;	/* bits: trace 0, bitmap 16-31 */
	struct i387_struct i387;
};

/*
 * task_struct - Task structure
 * 
 * Original: Kernel task structure with all process information.
 * Microkernel: Template for task information managed by process server.
 * Some fields are now capability IDs rather than direct pointers.
 */
struct task_struct {
/* these are hardcoded - don't touch */
	long state;	/* -1 unrunnable, 0 runnable, >0 stopped */
	long counter;
	long priority;
	long signal;
	struct sigaction sigaction[32];
	long blocked;	/* bitmap of masked signals */
/* various fields */
	int exit_code;
	unsigned long start_code,end_code,end_data,brk,start_stack;
	long pid,father,pgrp,session,leader;
	unsigned short uid,euid,suid;
	unsigned short gid,egid,sgid;
	long alarm;
	long utime,stime,cutime,cstime,start_time;
	unsigned short used_math;
/* file system info */
	int tty;		/* -1 if no tty, so it must be signed */
	unsigned short umask;
	struct m_inode * pwd;
	struct m_inode * root;
	struct m_inode * executable;
	unsigned long close_on_exec;
	struct file * filp[NR_OPEN];
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
	struct desc_struct ldt[3];
/* tss for this task */
	struct tss_struct tss;

/*=============================================================================
 * MICROKERNEL EXTENSIONS
 *============================================================================*/
	/* Capability fields */
	capability_t caps;		/* Task capabilities */
	unsigned int port_space;	/* IPC port space */
	unsigned int mem_space;	/* Memory capability space */
	unsigned int server_id;		/* Server this task belongs to */
	
	/* IPC fields */
	unsigned int reply_port;	/* Port for pending reply */
	unsigned int wait_port;		/* Port task is waiting on */
	unsigned long ipc_timeout;	/* IPC timeout */
	
	/* Debug fields */
	unsigned int debug_flags;	/* Debug flags */
};

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x9ffff (=640kB)
 * 
 * Microkernel version: Now includes capability initialization.
 */
#define INIT_TASK \
/* state etc */	{ 0,15,15, \
/* signals */	0,{{},},0, \
/* ec,brk... */	0,0,0,0,0,0, \
/* pid etc.. */	0,-1,0,0,0, \
/* uid etc */	0,0,0,0,0,0, \
/* alarm */	0,0,0,0,0,0, \
/* math */	0, \
/* fs info */	-1,0022,NULL,NULL,NULL,0, \
/* filp */	{NULL,}, \
	{ \
		{0,0}, \
/* ldt */	{0x9f,0xc0fa00}, \
		{0x9f,0xc0f200}, \
	}, \
/*tss*/	{0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&pg_dir,\
	 0,0,0,0,0,0,0,0, \
	 0,0,0x17,0x17,0x17,0x17,0x17,0x17, \
	 _LDT(0),0x80000000, \
		{} \
	}, \
/* microkernel extensions */ \
	CAP_ALL,		/* Kernel has all capabilities */ \
	0,			/* port_space */ \
	0,			/* mem_space */ \
	0,			/* server_id */ \
	0,			/* reply_port */ \
	0,			/* wait_port */ \
	0,			/* ipc_timeout */ \
	0			/* debug_flags */ \
}

/*=============================================================================
 * ORIGINAL EXTERNAL VARIABLES (Now managed by process server)
 *============================================================================*/

extern struct task_struct *task[NR_TASKS];
extern struct task_struct *last_task_used_math;
extern struct task_struct *current;
extern long volatile jiffies;
extern long startup_time;

#define CURRENT_TIME (startup_time+jiffies/HZ)

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_SCHED_INIT		0x1600	/* Initialize scheduler */
#define MSG_SCHED_SCHEDULE	0x1601	/* Run scheduler */
#define MSG_SCHED_SWITCH_TO	0x1602	/* Switch to task */
#define MSG_SCHED_ADD_TIMER	0x1603	/* Add timer */
#define MSG_SCHED_SLEEP_ON	0x1604	/* Sleep on wait queue */
#define MSG_SCHED_INTR_SLEEP	0x1605	/* Interruptible sleep */
#define MSG_SCHED_WAKE_UP	0x1606	/* Wake up task */
#define MSG_SCHED_GET_INFO	0x1607	/* Get task info */
#define MSG_SCHED_SET_PRIO	0x1608	/* Set task priority */
#define MSG_SCHED_YIELD		0x1609	/* Yield CPU */
#define MSG_SCHED_REPLY		0x160A	/* Reply from scheduler */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_sched_task {
	struct mk_msg_header header;
	unsigned int task_id;		/* Task ID */
	unsigned long param;		/* Parameter (timeout, etc) */
	struct task_struct **p;		/* Wait queue */
	unsigned int task_id2;		/* Second task ID */
	capability_t caps;		/* Caller capabilities */
};

struct msg_sched_timer {
	struct mk_msg_header header;
	long jiffies;			/* Timer delay */
	void (*fn)(void);		/* Timer function */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
};

struct msg_sched_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		struct task_struct *task;	/* Task pointer */
		unsigned long value;		/* Return value */
	} data;
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * CAPABILITY FLAGS
 *============================================================================*/

#define CAP_SCHED_ADMIN	0x0001	/* Can administer scheduling */
#define CAP_SCHED_SETPID	0x0002	/* Can set process IDs */
#define CAP_SCHED_SETPRIO	0x0004	/* Can set priority */

/*=============================================================================
 * ORIGINAL FUNCTION PROTOTYPES (Now stubs that send IPC)
 *============================================================================*/

extern int copy_page_tables(unsigned long from, unsigned long to, long size);
extern int free_page_tables(unsigned long from, unsigned long size);

extern void sched_init(void);
extern void schedule(void);
extern void trap_init(void);
#ifndef PANIC
void panic(const char * str);
#endif
extern int tty_write(unsigned minor,char * buf,int count);

typedef int (*fn_ptr)();

extern void add_timer(long jiffies, void (*fn)(void));
extern void sleep_on(struct task_struct ** p);
extern void interruptible_sleep_on(struct task_struct ** p);
extern void wake_up(struct task_struct ** p);

/*=============================================================================
 * TASK MANAGEMENT FUNCTIONS
 *============================================================================*/

/**
 * sched_init - Initialize scheduler
 * 
 * Sends request to process server to initialize scheduling.
 */
static inline void sched_init(void)
{
	struct msg_sched_task msg;
	
	if (!(current_capability & CAP_SCHED_ADMIN))
		return;

	msg.header.msg_id = MSG_SCHED_INIT;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
}

/**
 * schedule - Run scheduler
 * 
 * Yields CPU and lets process server choose next task.
 */
static inline void schedule(void)
{
	struct msg_sched_task msg;
	struct msg_sched_reply reply;
	unsigned int reply_size = sizeof(reply);

	msg.header.msg_id = MSG_SCHED_SCHEDULE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	if (mk_msg_send(kernel_state->process_server, &msg, sizeof(msg)) == 0) {
		mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	}
}

/**
 * add_timer - Add a kernel timer
 * @jiffies: Number of jiffies to wait
 * @fn: Function to call
 * 
 * Sends timer request to process server.
 */
static inline void add_timer(long jiffies, void (*fn)(void))
{
	struct msg_sched_timer msg;
	
	msg.header.msg_id = MSG_SCHED_ADD_TIMER;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.jiffies = jiffies;
	msg.fn = fn;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
}

/**
 * sleep_on - Sleep on a wait queue
 * @p: Wait queue head
 */
static inline void sleep_on(struct task_struct ** p)
{
	struct msg_sched_task msg;
	
	msg.header.msg_id = MSG_SCHED_SLEEP_ON;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	msg.p = p;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
	/* Will block until woken */
}

/**
 * interruptible_sleep_on - Interruptible sleep on wait queue
 * @p: Wait queue head
 */
static inline void interruptible_sleep_on(struct task_struct ** p)
{
	struct msg_sched_task msg;
	
	msg.header.msg_id = MSG_SCHED_INTR_SLEEP;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	msg.p = p;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
}

/**
 * wake_up - Wake up tasks on wait queue
 * @p: Wait queue head
 */
static inline void wake_up(struct task_struct ** p)
{
	struct msg_sched_task msg;
	
	msg.header.msg_id = MSG_SCHED_WAKE_UP;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.p = p;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
}

/*=============================================================================
 * CONTEXT SWITCHING MACROS (Now using IPC)
 *============================================================================*/

/*
 * Entry into gdt where to find first TSS. 0-nul, 1-cs, 2-ds, 3-syscall
 * 4-TSS0, 5-LDT0, 6-TSS1 etc ...
 */
#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))

/*
 * These macros now communicate with process server for context switching
 */
#define ltr(n) \
do { \
	struct msg_sched_task msg; \
	msg.header.msg_id = MSG_SCHED_LTR; \
	msg.header.sender_port = kernel_state->kernel_port; \
	msg.header.reply_port = 0; \
	msg.header.size = sizeof(msg); \
	msg.task_id = (n); \
	msg.caps = current_capability; \
	mk_msg_send(kernel_state->process_server, &msg, sizeof(msg)); \
} while (0)

#define lldt(n) \
do { \
	struct msg_sched_task msg; \
	msg.header.msg_id = MSG_SCHED_LLDT; \
	msg.header.sender_port = kernel_state->kernel_port; \
	msg.header.reply_port = 0; \
	msg.header.size = sizeof(msg); \
	msg.task_id = (n); \
	msg.caps = current_capability; \
	mk_msg_send(kernel_state->process_server, &msg, sizeof(msg)); \
} while (0)

#define str(n) \
({ \
	struct msg_sched_task msg; \
	struct msg_sched_reply reply; \
	unsigned int reply_size = sizeof(reply); \
	msg.header.msg_id = MSG_SCHED_STR; \
	msg.header.sender_port = kernel_state->kernel_port; \
	msg.header.reply_port = kernel_state->kernel_port; \
	msg.header.size = sizeof(msg); \
	msg.task_id = 0; \
	msg.caps = current_capability; \
	if (mk_msg_send(kernel_state->process_server, &msg, sizeof(msg)) == 0) { \
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) { \
			(n) = reply.data.value; \
		} \
	} \
})

/**
 * switch_to - Switch to another task
 * @n: Task number to switch to
 * 
 * Sends IPC to process server to perform context switch.
 * The process server saves current task state and loads new task.
 */
#define switch_to(n) \
do { \
	struct msg_sched_task msg; \
	struct msg_sched_reply reply; \
	unsigned int reply_size = sizeof(reply); \
	\
	/* Don't switch to current task */ \
	if ((n) == kernel_state->current_task) \
		break; \
	\
	msg.header.msg_id = MSG_SCHED_SWITCH_TO; \
	msg.header.sender_port = kernel_state->kernel_port; \
	msg.header.reply_port = kernel_state->kernel_port; \
	msg.header.size = sizeof(msg); \
	msg.task_id = (n); \
	msg.task_id2 = kernel_state->current_task; \
	msg.caps = current_capability; \
	\
	if (mk_msg_send(kernel_state->process_server, &msg, sizeof(msg)) == 0) { \
		mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size); \
		/* Update current task pointer */ \
		if (reply.result == 0) { \
			current = task[n]; \
		} \
	} \
} while (0)

/*=============================================================================
 * MEMORY MANAGEMENT FUNCTIONS
 *============================================================================*/

extern int copy_page_tables(unsigned long from, unsigned long to, long size);
extern int free_page_tables(unsigned long from, unsigned long size);

/**
 * copy_page_tables - Copy page tables (now via memory server)
 */
static inline int copy_page_tables(unsigned long from, unsigned long to, long size)
{
	struct msg_mem_copy_tables msg;
	struct msg_mem_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	msg.header.msg_id = MSG_MEM_COPY_TABLES;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	msg.from = from;
	msg.to = to;
	msg.size = size;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) < 0)
		return -1;

	if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0)
		return -1;

	return reply.result;
}

/**
 * free_page_tables - Free page tables (via memory server)
 */
static inline int free_page_tables(unsigned long from, unsigned long size)
{
	struct msg_mem_free_tables msg;
	struct msg_mem_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	msg.header.msg_id = MSG_MEM_FREE_TABLES;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	msg.from = from;
	msg.size = size;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) < 0)
		return -1;

	if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0)
		return -1;

	return reply.result;
}

/*=============================================================================
 * SEGMENT DESCRIPTOR MACROS (Now capability-aware)
 *============================================================================*/

#define PAGE_ALIGN(n) (((n)+0xfff)&0xfffff000)

#define _set_base(addr,base) \
do { \
	struct msg_set_base msg; \
	msg.header.msg_id = MSG_SET_BASE; \
	msg.header.sender_port = kernel_state->kernel_port; \
	msg.header.reply_port = 0; \
	msg.header.size = sizeof(msg); \
	msg.addr = (unsigned long)(addr); \
	msg.base = (base); \
	msg.task_id = kernel_state->current_task; \
	msg.caps = current_capability; \
	mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)); \
} while (0)

#define _set_limit(addr,limit) \
do { \
	struct msg_set_limit msg; \
	msg.header.msg_id = MSG_SET_LIMIT; \
	msg.header.sender_port = kernel_state->kernel_port; \
	msg.header.reply_port = 0; \
	msg.header.size = sizeof(msg); \
	msg.addr = (unsigned long)(addr); \
	msg.limit = (limit); \
	msg.task_id = kernel_state->current_task; \
	msg.caps = current_capability; \
	mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)); \
} while (0)

#define set_base(ldt,base) _set_base( ((char *)&(ldt)) , (base) )
#define set_limit(ldt,limit) _set_limit( ((char *)&(ldt)) , (limit-1)>>12 )

static inline unsigned long _get_base(char * addr)
{
	struct msg_get_base msg;
	struct msg_mem_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	msg.header.msg_id = MSG_GET_BASE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	msg.addr = (unsigned long)addr;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) < 0)
		return 0;

	if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0)
		return 0;

	return reply.data.value;
}

#define get_base(ldt) _get_base( ((char *)&(ldt)) )

#define get_limit(segment) ({ \
	struct msg_get_limit msg; \
	struct msg_mem_reply reply; \
	unsigned int reply_size = sizeof(reply); \
	msg.header.msg_id = MSG_GET_LIMIT; \
	msg.header.sender_port = kernel_state->kernel_port; \
	msg.header.reply_port = kernel_state->kernel_port; \
	msg.header.size = sizeof(msg); \
	msg.addr = (segment); \
	msg.task_id = kernel_state->current_task; \
	msg.caps = current_capability; \
	if (mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg)) == 0) { \
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) { \
			reply.data.value; \
		} else 0; \
	} else 0; \
})

/*=============================================================================
 * CAPABILITY REQUEST FUNCTION
 *============================================================================*/

static inline int request_sched_capability(unsigned int cap)
{
	struct msg_cap_request msg;
	struct msg_cap_reply reply;
	unsigned int reply_size = sizeof(reply);

	msg.header.msg_id = MSG_CAP_REQUEST_SCHED;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.task_id = kernel_state->current_task;
	msg.requested_cap = cap;

	if (mk_msg_send(kernel_state->process_server, &msg, sizeof(msg)) == 0) {
		if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) == 0) {
			if (reply.result == 0) {
				current_capability |= cap;
				return 0;
			}
		}
	}
	return -1;
}

#endif /* _SCHED_H */
