/*
* HISTORY
* $Log: sched.c,v $
* Revision 1.1 2026/02/14 20:30:45 pedro
* Microkernel version of scheduler.
* All scheduling operations now send IPC to process server.
* Timer interrupts handled by system server.
* Floppy operations moved to device server.
* [2026/02/14 pedro]
*/

/*
* File: kernel/sched.c
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* Scheduler for microkernel architecture.
*
* Original Linux 0.11: Direct kernel scheduling with TSS switching.
* Microkernel version: All scheduling operations are delegated to the
* process server via IPC. The kernel only maintains minimal state and
* forwards scheduling requests.
*
* The actual task lists, counters, and scheduling algorithm now reside
* in the process server. This file contains stubs that communicate with
* that server.
*/

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>
#include <signal.h>

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES (Additional)
 *============================================================================*/

#define MSG_SCHED_ALARM		0x1700	/* Handle alarm */
#define MSG_SCHED_PAUSE		0x1701	/* Sys_pause */
#define MSG_SCHED_NICE		0x1702	/* Sys_nice */
#define MSG_SCHED_GETPID		0x1703	/* Get PID */
#define MSG_SCHED_GETPPID		0x1704	/* Get PPID */
#define MSG_SCHED_GETUID		0x1705	/* Get UID */
#define MSG_SCHED_GETEUID		0x1706	/* Get EUID */
#define MSG_SCHED_GETGID		0x1707	/* Get GID */
#define MSG_SCHED_GETEGID		0x1708	/* Get EGID */
#define MSG_SCHED_SHOW_TASK	0x1709	/* Show task info */
#define MSG_SCHED_SHOW_STAT	0x170A	/* Show statistics */
#define MSG_SCHED_MATH_RESTORE	0x170B	/* Restore math state */
#define MSG_SCHED_TIMER		0x170C	/* Timer interrupt */
#define MSG_SCHED_FLOPPY_TIMER	0x170D	/* Floppy timer */
#define MSG_SCHED_ADD_TIMER	0x170E	/* Add timer */
#define MSG_SCHED_DO_TIMER	0x170F	/* Do timer work */

/*=============================================================================
 * ORIGINAL GLOBAL VARIABLES (Now mostly managed by process server)
 *============================================================================*/

long volatile jiffies = 0;
long startup_time = 0;

/*
 * current - Pointer to current task structure
 * 
 * In microkernel mode, this is a cache of the current task info.
 * The real task state is in the process server.
 */
struct task_struct *current = NULL;

struct task_struct *last_task_used_math = NULL;

/*
 * task array - Now just a local cache of task pointers
 * The actual task list is in the process server.
 */
struct task_struct *task[NR_TASKS] = {NULL,};

long user_stack [PAGE_SIZE>>2];

struct {
	long * a;
	short b;
} stack_start = { &user_stack [PAGE_SIZE>>2] , 0x10 };

/*=============================================================================
 * MICROKERNEL STATE
 *============================================================================*/

/* Cache of current task info - updated via IPC */
static struct task_struct current_cache;

/* Flag to indicate if we've initialized */
static int sched_initialized = 0;

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * sched_request - Send scheduling request to process server
 * @msg_id: Message ID
 * @param: Parameter
 * @need_reply: Whether to wait for reply
 * 
 * Returns result from process server.
 */
static inline long sched_request(unsigned int msg_id, unsigned long param,
                                   int need_reply)
{
	struct msg_sched_task msg;
	struct msg_sched_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	/* Check scheduling capability */
	if (!(current_capability & CAP_SCHED_ADMIN) && 
	    msg_id != MSG_SCHED_GETPID &&
	    msg_id != MSG_SCHED_GETPPID &&
	    msg_id != MSG_SCHED_GETUID &&
	    msg_id != MSG_SCHED_GETGID) {
		return -1;
	}

	msg.header.msg_id = msg_id;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = need_reply ? kernel_state->kernel_port : 0;
	msg.header.size = sizeof(msg);
	
	msg.task_id = kernel_state->current_task;
	msg.param = param;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
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
 * TASK INFORMATION FUNCTIONS
 *============================================================================*/

void show_task(int nr, struct task_struct * p)
{
	struct msg_sched_task msg;
	struct msg_sched_reply reply;
	unsigned int reply_size = sizeof(reply);
	int i, j = 4096 - sizeof(struct task_struct);

	/* Request task info from process server */
	msg.header.msg_id = MSG_SCHED_SHOW_TASK;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	msg.task_id = nr;
	msg.param = (unsigned long)p;
	msg.caps = current_capability;

	if (mk_msg_send(kernel_state->process_server, &msg, sizeof(msg)) < 0)
		return;

	if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0)
		return;

	/* Print task information */
	printk("%d: pid=%d, state=%d, ", nr, reply.data.value >> 16, 
	       reply.data.value & 0xFFFF);
	
	/* Stack usage info (simplified) */
	i = 0;
	while (i < j && !((char *)(p + 1))[i])
		i++;
	printk("%d (of %d) chars free in kernel stack\n\r", i, j);
}

void show_stat(void)
{
	int i;

	printk("Task statistics (from process server):\n");
	for (i = 0; i < NR_TASKS; i++) {
		if (task[i])
			show_task(i, task[i]);
	}
}

/*=============================================================================
 * MATH STATE HANDLING (Now via IPC)
 *============================================================================*/

void math_state_restore(void)
{
	struct msg_sched_task msg;
	
	msg.header.msg_id = MSG_SCHED_MATH_RESTORE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
	
	/* Update local cache */
	last_task_used_math = current;
}

/*=============================================================================
 * SCHEDULER
 *============================================================================*/

void schedule(void)
{
	struct msg_sched_task msg;
	struct msg_sched_reply reply;
	unsigned int reply_size = sizeof(reply);
	int next_task;

	/* Send schedule request to process server */
	msg.header.msg_id = MSG_SCHED_SCHEDULE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	if (mk_msg_send(kernel_state->process_server, &msg, sizeof(msg)) < 0)
		return;

	if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0)
		return;

	next_task = reply.result;

	/* If we need to switch tasks, do it */
	if (next_task >= 0 && next_task != kernel_state->current_task) {
		/* Update current task pointer */
		current = task[next_task];
		kernel_state->current_task = next_task;
		
		/* Perform context switch (would be handled by server) */
		/* In a real microkernel, the server would have already
		 * switched the task context before replying */
	}
}

/*=============================================================================
 * SYSTEM CALL STUBS
 *============================================================================*/

int sys_pause(void)
{
	return sched_request(MSG_SCHED_PAUSE, 0, 1);
}

void sleep_on(struct task_struct **p)
{
	struct msg_sched_task msg;
	
	if (!p)
		return;
	
	/* Task 0 (idle) shouldn't sleep */
	if (current == &init_task.task) {
		printk("task[0] trying to sleep - ignored\n");
		return;
	}

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

void interruptible_sleep_on(struct task_struct **p)
{
	struct msg_sched_task msg;
	
	if (!p)
		return;
	
	if (current == &init_task.task) {
		printk("task[0] trying to sleep - ignored\n");
		return;
	}

	msg.header.msg_id = MSG_SCHED_INTR_SLEEP;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	msg.p = p;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
}

void wake_up(struct task_struct **p)
{
	struct msg_sched_task msg;
	
	if (!p || !*p)
		return;

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
 * FLOPPY TIMER FUNCTIONS (Now via device server)
 *============================================================================*/

static struct task_struct * wait_motor[4] = {NULL, NULL, NULL, NULL};
static int mon_timer[4] = {0, 0, 0, 0};
static int moff_timer[4] = {0, 0, 0, 0};
unsigned char current_DOR = 0x0C;

int ticks_to_floppy_on(unsigned int nr)
{
	struct msg_floppy_timer msg;
	struct msg_floppy_reply reply;
	unsigned int reply_size = sizeof(reply);
	
	msg.header.msg_id = MSG_FLOPPY_TICKS_ON;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	msg.drive = nr;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	if (mk_msg_send(kernel_state->device_server, &msg, sizeof(msg)) < 0)
		return 0;

	if (mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size) < 0)
		return 0;

	return reply.result;
}

void floppy_on(unsigned int nr)
{
	struct msg_floppy_on msg;
	
	msg.header.msg_id = MSG_FLOPPY_ON;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.drive = nr;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
}

void floppy_off(unsigned int nr)
{
	struct msg_floppy_off msg;
	
	msg.header.msg_id = MSG_FLOPPY_OFF;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.drive = nr;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
}

void do_floppy_timer(void)
{
	struct msg_floppy_timer msg;
	
	msg.header.msg_id = MSG_FLOPPY_TIMER;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->device_server, &msg, sizeof(msg));
}

/*=============================================================================
 * TIMER FUNCTIONS
 *============================================================================*/

#define TIME_REQUESTS 64

static struct timer_list {
	long jiffies;
	void (*fn)();
	struct timer_list * next;
} timer_list[TIME_REQUESTS], * next_timer = NULL;

void add_timer(long jiffies, void (*fn)(void))
{
	struct msg_sched_timer msg;
	
	if (!fn)
		return;

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

void do_timer(long cpl)
{
	struct msg_sched_timer msg;
	
	msg.header.msg_id = MSG_SCHED_DO_TIMER;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.jiffies = cpl;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));

	/* Update local jiffies count */
	jiffies++;

	/* Check if we need to reschedule */
	if (current) {
		current->counter--;
		if (current->counter <= 0 && cpl) {
			schedule();
		}
	}
}

/*=============================================================================
 * SYSTEM CALLS
 *============================================================================*/

int sys_alarm(long seconds)
{
	struct msg_sched_task msg;
	struct msg_sched_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	msg.header.msg_id = MSG_SCHED_ALARM;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	msg.task_id = kernel_state->current_task;
	msg.param = seconds;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return reply.result;
}

int sys_getpid(void)
{
	/* Fast path - use cached value if available */
	if (current)
		return current->pid;
	
	return sched_request(MSG_SCHED_GETPID, 0, 1);
}

int sys_getppid(void)
{
	if (current)
		return current->father;
	
	return sched_request(MSG_SCHED_GETPPID, 0, 1);
}

int sys_getuid(void)
{
	if (current)
		return current->uid;
	
	return sched_request(MSG_SCHED_GETUID, 0, 1);
}

int sys_geteuid(void)
{
	if (current)
		return current->euid;
	
	return sched_request(MSG_SCHED_GETEUID, 0, 1);
}

int sys_getgid(void)
{
	if (current)
		return current->gid;
	
	return sched_request(MSG_SCHED_GETGID, 0, 1);
}

int sys_getegid(void)
{
	if (current)
		return current->egid;
	
	return sched_request(MSG_SCHED_GETEGID, 0, 1);
}

int sys_nice(long increment)
{
	struct msg_sched_task msg;
	struct msg_sched_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	msg.header.msg_id = MSG_SCHED_NICE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	msg.task_id = kernel_state->current_task;
	msg.param = increment;
	msg.caps = current_capability;

	result = mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	return reply.result;
}

/*=============================================================================
 * INITIALIZATION
 *============================================================================*/

void sched_init(void)
{
	struct msg_sched_init msg;
	struct msg_sched_reply reply;
	unsigned int reply_size = sizeof(reply);
	int i;

	printk("Initializing microkernel scheduler...\n");

	/* Initialize local task array */
	for (i = 0; i < NR_TASKS; i++) {
		task[i] = NULL;
	}

	/* Initialize current task */
	current = &init_task.task;
	task[0] = current;
	kernel_state->current_task = 0;

	/* Set initial capabilities */
	current_capability = CAP_ALL;

	/* Send initialization message to process server */
	msg.header.msg_id = MSG_SCHED_INIT;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	msg.task_id = 0;
	msg.caps = current_capability;

	if (mk_msg_send(kernel_state->process_server, &msg, sizeof(msg)) == 0) {
		mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	}

	/* Set up timer interrupt (now handled by system server) */
	outb_p(0x36, 0x43);		/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff, 0x40);	/* LSB */
	outb(LATCH >> 8, 0x40);		/* MSB */

	/* Enable timer interrupt in PIC */
	outb(inb_p(0x21) & ~0x01, 0x21);

	/* Set system call gate */
	set_system_gate(0x80, &system_call);

	sched_initialized = 1;
	
	printk("Scheduler initialized.\n");
}
