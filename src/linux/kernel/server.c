/*
* HISTORY
* $Log: server.c,v $
* Revision 1.1 2026/02/15 17:15:30 pedro
* Microkernel server implementations.
* All system servers in one file for simplicity.
* Each server runs as a separate process handling IPC requests.
* [2026/02/15 pedro]
*/

/*
 * File: kernel/server.c
 * Author: Pedro Emanuel (microkernel implementation)
 * Date: 2026/02/15
 *
 * Server implementations for microkernel architecture.
 *
 * This file contains the main loop for each server in the system:
 * - Memory server: Manages physical pages and page tables
 * - Process server: Manages tasks, scheduling, and signals
 * - File server: Manages file systems and file operations
 * - Device server: Manages hardware devices (disk, floppy, etc.)
 * - Time server: Manages system time and timers
 * - Signal server: Manages signal delivery
 * - Console server: Manages console output
 * - Log server: Manages system logging
 * - System server: Manages system configuration and info
 *
 * Each server runs in its own process, receiving IPC messages and
 * sending replies. They communicate only via IPC ports.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/tty.h>
#include <linux/fdreg.h>
#include <linux/hdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

/*=============================================================================
 * COMMON SERVER UTILITIES
 *============================================================================*/

/* Server port definitions (must match main.c) */
#define PORT_MEMORY		0x0003
#define PORT_PROCESS		0x0004
#define PORT_FILE		0x0005
#define PORT_DEVICE		0x0006
#define PORT_SIGNAL		0x0007
#define PORT_TERMINAL		0x0008
#define PORT_TIME		0x0009
#define PORT_SYSTEM		0x000A
#define PORT_CONSOLE		0x000B
#define PORT_LOG		0x000C

/* Maximum message size */
#define MAX_MSG_SIZE		4096

/* Server state structure */
struct server_state {
	unsigned int server_id;
	unsigned int port;
	unsigned int running;
	unsigned int requests_handled;
	unsigned long start_time;
};

/**
 * send_reply - Send reply to client
 * @reply_port: Port to send reply to
 * @msg_id: Message ID being replied to
 * @result: Result code
 * @data: Optional data
 * @size: Size of data
 * 
 * Returns 0 on success, -1 on error.
 */
static int send_reply(unsigned int reply_port, unsigned int msg_id,
                       int result, void *data, unsigned int size)
{
	struct mk_msg_header reply;
	
	reply.msg_id = msg_id;
	reply.sender_port = 0;  /* Will be filled by kernel */
	reply.reply_port = 0;
	reply.size = sizeof(reply) + size;
	
	/* Would need to assemble full message with data */
	/* For now, simplified */
	
	return mk_msg_send(reply_port, &reply, sizeof(reply));
}

/**
 * get_task_capabilities - Get task's capabilities from process server
 * @task_id: Task ID
 * 
 * Returns capability mask, or 0 if task not found.
 */
static capability_t get_task_capabilities(unsigned int task_id)
{
	/* Would query process server */
	/* For now, assume all capabilities */
	return CAP_ALL;
}

/**
 * validate_capability - Check if task has required capability
 * @task_id: Task ID
 * @required: Required capability mask
 * 
 * Returns 1 if task has capability, 0 otherwise.
 */
static int validate_capability(unsigned int task_id, capability_t required)
{
	capability_t caps = get_task_capabilities(task_id);
	return (caps & required) == required;
}

/*=============================================================================
 * MEMORY SERVER
 *============================================================================*/

/*
 * Memory server manages physical pages, page tables, and memory objects.
 * It handles requests from:
 * - get_free_page(), free_page(), put_page()
 * - copy_page_tables(), free_page_tables()
 * - page fault handling (do_no_page, do_wp_page)
 */

/* Physical page management */
static struct {
	unsigned long addr;		/* Physical address */
	unsigned int ref_count;		/* Reference count */
	unsigned int flags;		/* Page flags */
	unsigned int owner;		/* Owner task ID */
	unsigned int object_id;		/* Memory object ID */
} physical_pages[PAGING_PAGES];

/* Page tables (simplified - in real implementation, per task) */
static unsigned long page_dir[1024] __attribute__((aligned(4096)));
static unsigned long page_tables[4][1024] __attribute__((aligned(4096)));

/* Memory objects */
struct memory_object {
	unsigned int obj_id;
	unsigned int size;
	unsigned int task;
	unsigned int ref_count;
	unsigned int copy_strategy;
	vm_prot_t default_prot;
};

static struct memory_object memory_objects[64];
static unsigned int next_object_id = 1;

/* Forward declarations */
static int mem_handle_get_free_page(struct msg_mem_page *msg, unsigned int reply_port);
static int mem_handle_put_page(struct msg_mem_page *msg, unsigned int reply_port);
static int mem_handle_free_page(struct msg_mem_page *msg, unsigned int reply_port);
static int mem_handle_copy_tables(struct msg_mem_page *msg, unsigned int reply_port);
static int mem_handle_free_tables(struct msg_mem_page *msg, unsigned int reply_port);
static int mem_handle_wp_page(struct msg_mem_page *msg, unsigned int reply_port);
static int mem_handle_no_page(struct msg_mem_page *msg, unsigned int reply_port);

/**
 * memory_server_main - Main loop for memory server
 */
void memory_server_main(void)
{
	struct mk_msg_header header;
	char buffer[MAX_MSG_SIZE];
	unsigned int size;
	int result;
	
	printk("Memory server started on port %d\n", PORT_MEMORY);
	
	while (1) {
		/* Receive request */
		size = MAX_MSG_SIZE;
		result = mk_msg_receive(PORT_MEMORY, &header, &size);
		if (result < 0)
			continue;
		
		/* Handle based on message ID */
		switch (header.msg_id) {
			case MSG_MEM_GET_FREE_PAGE:
				result = mem_handle_get_free_page((struct msg_mem_page *)&header,
				                                   header.reply_port);
				break;
				
			case MSG_MEM_PUT_PAGE:
				result = mem_handle_put_page((struct msg_mem_page *)&header,
				                                  header.reply_port);
				break;
				
			case MSG_MEM_FREE_PAGE:
				result = mem_handle_free_page((struct msg_mem_page *)&header,
				                                   header.reply_port);
				break;
				
			case MSG_MEM_COPY_PAGE_TABLES:
				result = mem_handle_copy_tables((struct msg_mem_page *)&header,
				                                     header.reply_port);
				break;
				
			case MSG_MEM_FREE_PAGE_TABLES:
				result = mem_handle_free_tables((struct msg_mem_page *)&header,
				                                     header.reply_port);
				break;
				
			case MSG_MEM_DO_WP_PAGE:
				result = mem_handle_wp_page((struct msg_mem_page *)&header,
				                              header.reply_port);
				break;
				
			case MSG_MEM_DO_NO_PAGE:
				result = mem_handle_no_page((struct msg_mem_page *)&header,
				                              header.reply_port);
				break;
				
			default:
				/* Unknown message */
				send_reply(header.reply_port, header.msg_id, -EINVAL, NULL, 0);
				break;
		}
	}
}

/* Memory server handlers */
static int mem_handle_get_free_page(struct msg_mem_page *msg, unsigned int reply_port)
{
	unsigned long page;
	int i;
	
	/* Validate capability */
	if (!validate_capability(msg->task_id, CAP_MEM_PAGE))
		return send_reply(reply_port, msg->header.msg_id, -EPERM, NULL, 0);
	
	/* Find free page */
	for (i = 0; i < PAGING_PAGES; i++) {
		if (physical_pages[i].ref_count == 0) {
			physical_pages[i].ref_count = 1;
			physical_pages[i].owner = msg->task_id;
			page = LOW_MEM + (i << 12);
			
			/* Zero the page */
			memset((void *)page, 0, 4096);
			
			return send_reply(reply_port, msg->header.msg_id, 0, &page, sizeof(page));
		}
	}
	
	/* Out of memory */
	return send_reply(reply_port, msg->header.msg_id, -ENOMEM, NULL, 0);
}

static int mem_handle_put_page(struct msg_mem_page *msg, unsigned int reply_port)
{
	unsigned long page = msg->page;
	unsigned long address = msg->address;
	unsigned long *page_table;
	unsigned long *dir_entry;
	int idx;
	
	/* Validate capability */
	if (!validate_capability(msg->task_id, CAP_MEM_PAGE))
		return send_reply(reply_port, msg->header.msg_id, -EPERM, NULL, 0);
	
	/* Validate page address */
	if (page < LOW_MEM || page >= HIGH_MEMORY)
		return send_reply(reply_port, msg->header.msg_id, -EINVAL, NULL, 0);
	
	/* Find or create page table */
	dir_entry = &page_dir[address >> 22];
	if (!(*dir_entry & 1)) {
		/* Need new page table */
		unsigned long new_table;
		int i;
		
		/* Find free page for page table */
		for (i = 0; i < PAGING_PAGES; i++) {
			if (physical_pages[i].ref_count == 0) {
				new_table = LOW_MEM + (i << 12);
				physical_pages[i].ref_count = 1;
				physical_pages[i].owner = msg->task_id;
				memset((void *)new_table, 0, 4096);
				break;
			}
		}
		if (i == PAGING_PAGES)
			return send_reply(reply_port, msg->header.msg_id, -ENOMEM, NULL, 0);
		
		*dir_entry = new_table | 7;  /* Present, R/W, User */
	}
	
	page_table = (unsigned long *)(*dir_entry & 0xfffff000);
	idx = (address >> 12) & 0x3ff;
	
	/* Map the page */
	page_table[idx] = page | 7;  /* Present, R/W, User */
	
	/* Update page reference */
	physical_pages[(page - LOW_MEM) >> 12].ref_count++;
	
	return send_reply(reply_port, msg->header.msg_id, 0, &page, sizeof(page));
}

static int mem_handle_free_page(struct msg_mem_page *msg, unsigned int reply_port)
{
	unsigned long page = msg->page;
	int idx = (page - LOW_MEM) >> 12;
	
	if (idx >= 0 && idx < PAGING_PAGES) {
		if (physical_pages[idx].ref_count > 0)
			physical_pages[idx].ref_count--;
	}
	
	/* No reply needed for free_page (fire and forget) */
	return 0;
}

static int mem_handle_copy_tables(struct msg_mem_page *msg, unsigned int reply_port)
{
	/* Would copy page tables from from to to */
	return send_reply(reply_port, msg->header.msg_id, -ENOSYS, NULL, 0);
}

static int mem_handle_free_tables(struct msg_mem_page *msg, unsigned int reply_port)
{
	/* Would free page tables */
	return send_reply(reply_port, msg->header.msg_id, -ENOSYS, NULL, 0);
}

static int mem_handle_wp_page(struct msg_mem_page *msg, unsigned int reply_port)
{
	/* Would handle write-protect page fault */
	return send_reply(reply_port, msg->header.msg_id, -ENOSYS, NULL, 0);
}

static int mem_handle_no_page(struct msg_mem_page *msg, unsigned int reply_port)
{
	/* Would handle page fault (demand loading) */
	return send_reply(reply_port, msg->header.msg_id, -ENOSYS, NULL, 0);
}

/*=============================================================================
 * PROCESS SERVER
 *============================================================================*/

/*
 * Process server manages tasks, scheduling, and signals.
 * It handles:
 * - fork(), execve(), exit(), wait()
 * - Scheduling decisions
 * - Signal delivery
 */

#define MAX_TASKS 64

struct server_task {
	struct task_struct *task;	/* Task structure */
	unsigned int pid;		/* Process ID */
	unsigned int state;		/* Task state */
	unsigned int priority;		/* Priority */
	unsigned int counter;		/* Time counter */
	unsigned int signal;		/* Pending signals */
	unsigned int blocked;		/* Blocked signals */
	struct sigaction sigaction[32];	/* Signal handlers */
	unsigned long utime, stime;	/* CPU times */
	unsigned long start_time;	/* Start time */
	unsigned int father;		/* Parent PID */
	unsigned int pgrp;		/* Process group */
	unsigned int session;		/* Session */
	unsigned int leader;		/* Session leader flag */
	capability_t caps;		/* Task capabilities */
};

static struct server_task tasks[MAX_TASKS];
static unsigned int next_pid = 1;
static unsigned int current_task = 0;

/* Forward declarations */
static int proc_handle_fork(struct msg_sched_task *msg, unsigned int reply_port);
static int proc_handle_execve(struct msg_sched_task *msg, unsigned int reply_port);
static int proc_handle_exit(struct msg_exit_do_exit *msg, unsigned int reply_port);
static int proc_handle_waitpid(struct msg_exit_waitpid *msg, unsigned int reply_port);
static int proc_handle_kill(struct msg_exit_kill *msg, unsigned int reply_port);
static int proc_handle_signal(struct msg_signal_signal *msg, unsigned int reply_port);
static int proc_handle_sigaction(struct msg_signal_sigaction *msg, unsigned int reply_port);
static int proc_handle_schedule(struct msg_sched_task *msg, unsigned int reply_port);
static int proc_handle_getpid(struct msg_sched_task *msg, unsigned int reply_port);
static int proc_handle_getppid(struct msg_sched_task *msg, unsigned int reply_port);

/**
 * process_server_main - Main loop for process server
 */
void process_server_main(void)
{
	struct mk_msg_header header;
	char buffer[MAX_MSG_SIZE];
	unsigned int size;
	int result;
	
	printk("Process server started on port %d\n", PORT_PROCESS);
	
	/* Initialize task 0 (idle task) */
	memset(&tasks[0], 0, sizeof(struct server_task));
	tasks[0].pid = 0;
	tasks[0].state = TASK_RUNNING;
	tasks[0].priority = 15;
	tasks[0].counter = 15;
	tasks[0].caps = CAP_ALL;
	
	while (1) {
		size = MAX_MSG_SIZE;
		result = mk_msg_receive(PORT_PROCESS, &header, &size);
		if (result < 0)
			continue;
		
		switch (header.msg_id) {
			case MSG_SCHED_FORK:
			case MSG_SCHED_SYS_FORK:
				proc_handle_fork((struct msg_sched_task *)&header, header.reply_port);
				break;
				
			case MSG_SCHED_EXECVE:
				proc_handle_execve((struct msg_sched_task *)&header, header.reply_port);
				break;
				
			case MSG_EXIT_DO_EXIT:
				proc_handle_exit((struct msg_exit_do_exit *)&header, header.reply_port);
				break;
				
			case MSG_EXIT_WAITPID:
				proc_handle_waitpid((struct msg_exit_waitpid *)&header, header.reply_port);
				break;
				
			case MSG_EXIT_KILL:
				proc_handle_kill((struct msg_exit_kill *)&header, header.reply_port);
				break;
				
			case MSG_SIGNAL_SIGNAL:
				proc_handle_signal((struct msg_signal_signal *)&header, header.reply_port);
				break;
				
			case MSG_SIGNAL_SIGACTION:
				proc_handle_sigaction((struct msg_signal_sigaction *)&header, header.reply_port);
				break;
				
			case MSG_SCHED_SCHEDULE:
				proc_handle_schedule((struct msg_sched_task *)&header, header.reply_port);
				break;
				
			case MSG_SCHED_GETPID:
				proc_handle_getpid((struct msg_sched_task *)&header, header.reply_port);
				break;
				
			case MSG_SCHED_GETPPID:
				proc_handle_getppid((struct msg_sched_task *)&header, header.reply_port);
				break;
				
			default:
				send_reply(header.reply_port, header.msg_id, -EINVAL, NULL, 0);
				break;
		}
	}
}

/* Process server handlers */
static int proc_handle_fork(struct msg_sched_task *msg, unsigned int reply_port)
{
	int i, pid;
	struct server_task *parent, *child;
	
	/* Find free slot */
	for (i = 1; i < MAX_TASKS; i++) {
		if (tasks[i].pid == 0)
			break;
	}
	if (i == MAX_TASKS)
		return send_reply(reply_port, msg->header.msg_id, -EAGAIN, NULL, 0);
	
	parent = &tasks[current_task];
	child = &tasks[i];
	
	/* Copy parent task */
	memcpy(child, parent, sizeof(struct server_task));
	
	/* Set new PID */
	pid = next_pid++;
	child->pid = pid;
	child->father = parent->pid;
	child->state = TASK_RUNNING;
	child->counter = child->priority;
	child->utime = child->stime = 0;
	child->start_time = jiffies;
	
	/* Child gets copy of parent's capabilities */
	child->caps = parent->caps;
	
	return send_reply(reply_port, msg->header.msg_id, 0, &pid, sizeof(pid));
}

static int proc_handle_execve(struct msg_sched_task *msg, unsigned int reply_port)
{
	/* Would load new executable */
	return send_reply(reply_port, msg->header.msg_id, -ENOSYS, NULL, 0);
}

static int proc_handle_exit(struct msg_exit_do_exit *msg, unsigned int reply_port)
{
	struct server_task *task = &tasks[current_task];
	int i;
	
	task->state = TASK_ZOMBIE;
	task->exit_code = msg->code;
	
	/* Reparent children to init (task 1) */
	for (i = 0; i < MAX_TASKS; i++) {
		if (tasks[i].father == task->pid) {
			tasks[i].father = 1;
			if (tasks[i].state == TASK_ZOMBIE) {
				/* Send SIGCHLD to init */
				/* Would need to signal init */
			}
		}
	}
	
	/* Wake up father */
	/* Would need to wake up waiting task */
	
	return send_reply(reply_port, msg->header.msg_id, 0, NULL, 0);
}

static int proc_handle_waitpid(struct msg_exit_waitpid *msg, unsigned int reply_port)
{
	int i;
	struct server_task *child;
	int found = 0;
	
	for (i = 0; i < MAX_TASKS; i++) {
		child = &tasks[i];
		if (child->pid == 0 || child->father != tasks[current_task].pid)
			continue;
		
		if (msg->pid > 0 && child->pid != msg->pid)
			continue;
		
		found = 1;
		
		if (child->state == TASK_ZOMBIE) {
			unsigned long code = child->exit_code;
			int pid = child->pid;
			child->pid = 0;  /* Free slot */
			return send_reply(reply_port, msg->header.msg_id, pid, &code, sizeof(code));
		}
	}
	
	if (!found)
		return send_reply(reply_port, msg->header.msg_id, -ECHILD, NULL, 0);
	
	/* No zombie yet - would block */
	return send_reply(reply_port, msg->header.msg_id, -EAGAIN, NULL, 0);
}

static int proc_handle_kill(struct msg_exit_kill *msg, unsigned int reply_port)
{
	int i;
	struct server_task *target = NULL;
	
	/* Find target task */
	if (msg->pid > 0) {
		for (i = 0; i < MAX_TASKS; i++) {
			if (tasks[i].pid == msg->pid) {
				target = &tasks[i];
				break;
			}
		}
	} else {
		/* Would handle process groups */
		return send_reply(reply_port, msg->header.msg_id, -ENOSYS, NULL, 0);
	}
	
	if (!target)
		return send_reply(reply_port, msg->header.msg_id, -ESRCH, NULL, 0);
	
	/* Check permissions */
	if (!validate_capability(msg->task_id, CAP_EXIT_KILL) &&
	    tasks[msg->task_id].euid != target->euid) {
		return send_reply(reply_port, msg->header.msg_id, -EPERM, NULL, 0);
	}
	
	/* Send signal */
	if (msg->sig > 0 && msg->sig <= 32) {
		target->signal |= (1 << (msg->sig - 1));
	}
	
	return send_reply(reply_port, msg->header.msg_id, 0, NULL, 0);
}

static int proc_handle_signal(struct msg_signal_signal *msg, unsigned int reply_port)
{
	struct server_task *task = &tasks[current_task];
	unsigned long old_handler;
	
	if (msg->signum < 1 || msg->signum > 32 || msg->signum == SIGKILL)
		return send_reply(reply_port, msg->header.msg_id, -EINVAL, NULL, 0);
	
	old_handler = (unsigned long)task->sigaction[msg->signum-1].sa_handler;
	
	task->sigaction[msg->signum-1].sa_handler = (void (*)(int))msg->handler;
	task->sigaction[msg->signum-1].sa_mask = 0;
	task->sigaction[msg->signum-1].sa_flags = SA_ONESHOT | SA_NOMASK;
	task->sigaction[msg->signum-1].sa_restorer = (void (*)(void))msg->restorer;
	
	return send_reply(reply_port, msg->header.msg_id, 0, &old_handler, sizeof(old_handler));
}

static int proc_handle_sigaction(struct msg_signal_sigaction *msg, unsigned int reply_port)
{
	/* Would set signal action */
	return send_reply(reply_port, msg->header.msg_id, -ENOSYS, NULL, 0);
}

static int proc_handle_schedule(struct msg_sched_task *msg, unsigned int reply_port)
{
	int i, next = -1;
	int max_counter = -1;
	
	/* Simple round-robin scheduler */
	for (i = 0; i < MAX_TASKS; i++) {
		if (tasks[i].state == TASK_RUNNING) {
			if (tasks[i].counter > max_counter) {
				max_counter = tasks[i].counter;
				next = i;
			}
		}
	}
	
	if (next >= 0) {
		/* Give timeslice to next task */
		current_task = next;
		tasks[next].counter--;
	}
	
	return send_reply(reply_port, msg->header.msg_id, next, NULL, 0);
}

static int proc_handle_getpid(struct msg_sched_task *msg, unsigned int reply_port)
{
	int pid = tasks[current_task].pid;
	return send_reply(reply_port, msg->header.msg_id, 0, &pid, sizeof(pid));
}

static int proc_handle_getppid(struct msg_sched_task *msg, unsigned int reply_port)
{
	int ppid = tasks[current_task].father;
	return send_reply(reply_port, msg->header.msg_id, 0, &ppid, sizeof(ppid));
}

/*=============================================================================
 * DEVICE SERVER
 *============================================================================*/

/*
 * Device server manages hardware devices:
 * - Hard disk (IDE)
 * - Floppy disk
 * - Timer (PIT)
 * - Keyboard
 * - Serial ports
 */

/* Hard disk state */
struct hd_state {
	unsigned int present;
	unsigned int cylinders;
	unsigned int heads;
	unsigned int sectors;
	unsigned int irq;
	unsigned int io_base;
};

static struct hd_state hd_drives[4];

/* Floppy state */
struct floppy_state {
	unsigned int present;
	unsigned int motor_on;
	unsigned int current_track;
};

static struct floppy_state floppy_drives[4];

/* Timer state */
static unsigned long timer_ticks = 0;

/* Forward declarations */
static int dev_handle_hd_read(struct msg_hd_rw *msg, unsigned int reply_port);
static int dev_handle_hd_write(struct msg_hd_rw *msg, unsigned int reply_port);
static int dev_handle_floppy_on(struct msg_floppy_request *msg, unsigned int reply_port);
static int dev_handle_floppy_off(struct msg_floppy_request *msg, unsigned int reply_port);
static int dev_handle_floppy_read(struct msg_floppy_request *msg, unsigned int reply_port);
static int dev_handle_floppy_write(struct msg_floppy_request *msg, unsigned int reply_port);

/**
 * device_server_main - Main loop for device server
 */
void device_server_main(void)
{
	struct mk_msg_header header;
	char buffer[MAX_MSG_SIZE];
	unsigned int size;
	int result;
	
	printk("Device server started on port %d\n", PORT_DEVICE);
	
	/* Initialize hardware */
	outb_p(0x36, 0x43);		/* Timer */
	outb_p(LATCH & 0xff, 0x40);
	outb(LATCH >> 8, 0x40);
	
	while (1) {
		size = MAX_MSG_SIZE;
		result = mk_msg_receive(PORT_DEVICE, &header, &size);
		if (result < 0)
			continue;
		
		switch (header.msg_id) {
			case MSG_HD_READ:
				dev_handle_hd_read((struct msg_hd_rw *)&header, header.reply_port);
				break;
				
			case MSG_HD_WRITE:
				dev_handle_hd_write((struct msg_hd_rw *)&header, header.reply_port);
				break;
				
			case MSG_FLOPPY_ON:
				dev_handle_floppy_on((struct msg_floppy_request *)&header, header.reply_port);
				break;
				
			case MSG_FLOPPY_OFF:
				dev_handle_floppy_off((struct msg_floppy_request *)&header, header.reply_port);
				break;
				
			case MSG_FLOPPY_READ:
				dev_handle_floppy_read((struct msg_floppy_request *)&header, header.reply_port);
				break;
				
			case MSG_FLOPPY_WRITE:
				dev_handle_floppy_write((struct msg_floppy_request *)&header, header.reply_port);
				break;
				
			default:
				send_reply(header.reply_port, header.msg_id, -EINVAL, NULL, 0);
				break;
		}
	}
}

/* Device server handlers (simplified - would do actual I/O) */
static int dev_handle_hd_read(struct msg_hd_rw *msg, unsigned int reply_port)
{
	/* Would read from hard disk */
	/* For now, just succeed */
	return send_reply(reply_port, msg->header.msg_id, 0, &msg->count, sizeof(msg->count));
}

static int dev_handle_hd_write(struct msg_hd_rw *msg, unsigned int reply_port)
{
	/* Would write to hard disk */
	return send_reply(reply_port, msg->header.msg_id, 0, &msg->count, sizeof(msg->count));
}

static int dev_handle_floppy_on(struct msg_floppy_request *msg, unsigned int reply_port)
{
	/* Turn on floppy motor */
	outb(0x1C, FD_DOR);  /* Example */
	return send_reply(reply_port, msg->header.msg_id, 0, NULL, 0);
}

static int dev_handle_floppy_off(struct msg_floppy_request *msg, unsigned int reply_port)
{
	/* Turn off floppy motor */
	outb(0x0C, FD_DOR);  /* Example */
	return send_reply(reply_port, msg->header.msg_id, 0, NULL, 0);
}

static int dev_handle_floppy_read(struct msg_floppy_request *msg, unsigned int reply_port)
{
	/* Would read from floppy */
	return send_reply(reply_port, msg->header.msg_id, 0, &msg->params.rw.count, sizeof(msg->params.rw.count));
}

static int dev_handle_floppy_write(struct msg_floppy_request *msg, unsigned int reply_port)
{
	/* Would write to floppy */
	return send_reply(reply_port, msg->header.msg_id, 0, &msg->params.rw.count, sizeof(msg->params.rw.count));
}

/*=============================================================================
 * TIME SERVER
 *============================================================================*/

/*
 * Time server manages system time and timers.
 * It handles:
 * - time(), stime()
 * - gettimeofday()
 * - alarm timers
 * - interval timers
 */

static unsigned long system_time = 0;
static unsigned long boot_time = 0;

struct timer {
	unsigned long expires;
	unsigned int task_id;
	int signal;
	struct timer *next;
};

static struct timer *timers = NULL;

/**
 * time_server_main - Main loop for time server
 */
void time_server_main(void)
{
	struct mk_msg_header header;
	char buffer[MAX_MSG_SIZE];
	unsigned int size;
	int result;
	
	printk("Time server started on port %d\n", PORT_TIME);
	
	/* Initialize time from CMOS */
	boot_time = jiffies;
	system_time = startup_time;
	
	while (1) {
		size = MAX_MSG_SIZE;
		result = mk_msg_receive(PORT_TIME, &header, &size);
		if (result < 0)
			continue;
		
		switch (header.msg_id) {
			case MSG_SYS_TIME:
				/* Return current time */
				send_reply(header.reply_port, header.msg_id, 0, &system_time, sizeof(system_time));
				break;
				
			case MSG_SYS_STIME:
				/* Set system time (requires capability) */
				/* Would check CAP_SYS_TIME */
				system_time = *(unsigned long *)((char *)&header + sizeof(struct mk_msg_header));
				send_reply(header.reply_port, header.msg_id, 0, NULL, 0);
				break;
				
			case MSG_SCHED_ALARM:
				/* Set alarm for task */
				/* Would add timer */
				send_reply(header.reply_port, header.msg_id, 0, NULL, 0);
				break;
				
			default:
				send_reply(header.reply_port, header.msg_id, -EINVAL, NULL, 0);
				break;
		}
		
		/* Check timers */
		/* ... */
	}
}

/*=============================================================================
 * SIGNAL SERVER
 *============================================================================*/

/*
 * Signal server manages signal delivery.
 * It works closely with process server.
 */

/**
 * signal_server_main - Main loop for signal server
 */
void signal_server_main(void)
{
	struct mk_msg_header header;
	char buffer[MAX_MSG_SIZE];
	unsigned int size;
	int result;
	
	printk("Signal server started on port %d\n", PORT_SIGNAL);
	
	while (1) {
		size = MAX_MSG_SIZE;
		result = mk_msg_receive(PORT_SIGNAL, &header, &size);
		if (result < 0)
			continue;
		
		switch (header.msg_id) {
			case MSG_SIGNAL_SEND:
				/* Send signal to task */
				/* Would validate and forward to process server */
				send_reply(header.reply_port, header.msg_id, 0, NULL, 0);
				break;
				
			default:
				send_reply(header.reply_port, header.msg_id, -EINVAL, NULL, 0);
				break;
		}
	}
}

/*=============================================================================
 * CONSOLE SERVER
 *============================================================================*/

/*
 * Console server manages console output.
 */

static int console_buffer_pos = 0;
static char console_buffer[4096];

/**
 * console_server_main - Main loop for console server
 */
void console_server_main(void)
{
	struct mk_msg_header header;
	char buffer[MAX_MSG_SIZE];
	unsigned int size;
	int result;
	
	printk("Console server started on port %d\n", PORT_CONSOLE);
	
	while (1) {
		size = MAX_MSG_SIZE;
		result = mk_msg_receive(PORT_CONSOLE, &header, &size);
		if (result < 0)
			continue;
		
		if (header.msg_id == MSG_CONSOLE_WRITE) {
			struct msg_console_write *msg = (struct msg_console_write *)&header;
			
			/* Write to console */
			__asm__ __volatile__(
				"push %%fs\n\t"
				"push %%ds\n\t"
				"pop %%fs\n\t"
				"pushl %0\n\t"
				"pushl $msg->data\n\t"
				"pushl $0\n\t"
				"call tty_write\n\t"
				"addl $8,%%esp\n\t"
				"popl %0\n\t"
				"pop %%fs"
				: : "r" (msg->len) : "ax", "cx", "dx");
			
			/* Also add to log buffer */
			memcpy(console_buffer + console_buffer_pos, msg->data, msg->len);
			console_buffer_pos += msg->len;
			if (console_buffer_pos > 3000)
				console_buffer_pos = 0;
		}
	}
}

/*=============================================================================
 * LOG SERVER
 *============================================================================*/

/*
 * Log server manages system logging.
 */

#define LOG_BUFFER_SIZE 65536
static char log_buffer[LOG_BUFFER_SIZE];
static unsigned int log_head = 0;
static unsigned int log_tail = 0;

/**
 * log_server_main - Main loop for log server
 */
void log_server_main(void)
{
	struct mk_msg_header header;
	char buffer[MAX_MSG_SIZE];
	unsigned int size;
	int result;
	
	printk("Log server started on port %d\n", PORT_LOG);
	
	while (1) {
		size = MAX_MSG_SIZE;
		result = mk_msg_receive(PORT_LOG, &header, &size);
		if (result < 0)
			continue;
		
		if (header.msg_id == MSG_LOG_WRITE) {
			struct msg_log_write *msg = (struct msg_log_write *)&header;
			int i;
			
			/* Add to circular buffer */
			for (i = 0; i < msg->len; i++) {
				log_buffer[log_head] = msg->data[i];
				log_head = (log_head + 1) & (LOG_BUFFER_SIZE - 1);
				if (log_head == log_tail)
					log_tail = (log_tail + 1) & (LOG_BUFFER_SIZE - 1);
			}
		}
	}
}

/*=============================================================================
 * SYSTEM SERVER
 *============================================================================*/

/*
 * System server manages system configuration and information.
 * It handles:
 * - uname()
 * - sysinfo()
 * - system configuration
 */

struct utsname system_identity = {
	"MicroLinux",
	"localhost",
	"0.11-micro",
	"#1 SMP 2026",
	"i386"
};

/**
 * system_server_main - Main loop for system server
 */
void system_server_main(void)
{
	struct mk_msg_header header;
	char buffer[MAX_MSG_SIZE];
	unsigned int size;
	int result;
	
	printk("System server started on port %d\n", PORT_SYSTEM);
	
	while (1) {
		size = MAX_MSG_SIZE;
		result = mk_msg_receive(PORT_SYSTEM, &header, &size);
		if (result < 0)
			continue;
		
		switch (header.msg_id) {
			case MSG_SYS_UNAME:
				/* Return system identity */
				memcpy(buffer, &system_identity, sizeof(struct utsname));
				send_reply(header.reply_port, header.msg_id, 0, 
				          buffer, sizeof(struct utsname));
				break;
				
			case MSG_CONFIG_GET:
				/* Get configuration parameter */
				/* ... */
				send_reply(header.reply_port, header.msg_id, 0, NULL, 0);
				break;
				
			case MSG_PANIC_NOTIFY:
				/* Panic notification - log and maybe reboot */
				printk("PANIC received from task\n");
				send_reply(header.reply_port, header.msg_id, 0, NULL, 0);
				break;
				
			default:
				send_reply(header.reply_port, header.msg_id, -EINVAL, NULL, 0);
				break;
		}
	}
}
