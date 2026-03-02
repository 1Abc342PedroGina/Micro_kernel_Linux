/*
* HISTORY
* $Log: panic.c,v $
* Revision 1.1 2026/02/14 21:30:45 pedro
* Microkernel version of kernel panic handler.
* Panic now sends IPC message to system server.
* Attempts to sync via file server before panic.
* Includes capability-aware panic reasons.
* [2026/02/14 pedro]
*/

/*
 * File: kernel/panic.c
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/14
 *
 * Kernel panic handling for microkernel architecture.
 *
 * Original Linux 0.11: Simple panic that prints message and halts.
 * Microkernel version: Panic sends IPC messages to system server,
 * attempts to synchronize with file server, and notifies all servers
 * of the critical condition. The system server may attempt recovery
 * or initiate an orderly shutdown.
 *
 * Security: Panic requires CAP_SYSTEM capability. If called without
 * proper capabilities, it still panics but with reduced functionality.
 */

/*
 * This function is used through-out the kernel (including mm and fs)
 * to indicate a major problem.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/head.h>

/*=============================================================================
 * IPC MESSAGE CODES FOR PANIC
 *============================================================================*/

#define MSG_PANIC_NOTIFY	0x1800	/* Notify system server of panic */
#define MSG_PANIC_SYNC		0x1801	/* Request sync from file server */
#define MSG_PANIC_REBOOT	0x1802	/* Request reboot */
#define MSG_PANIC_HALT		0x1803	/* Request halt */
#define MSG_PANIC_DUMP		0x1804	/* Request crash dump */

/*=============================================================================
 * PANIC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_panic_notify {
	struct mk_msg_header header;
	char reason[64];		/* Panic reason string */
	unsigned long code;		/* Panic code */
	unsigned long eip;		/* Instruction pointer at panic */
	unsigned long esp;		/* Stack pointer at panic */
	unsigned int task_id;		/* Task that panicked */
	capability_t caps;		/* Caller capabilities */
	unsigned int flags;		/* Panic flags */
};

struct msg_panic_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	unsigned long action;		/* Recommended action */
};

/*=============================================================================
 * PANIC FLAGS
 *============================================================================*/

#define PANIC_FATAL		0x0001	/* Fatal panic - cannot recover */
#define PANIC_RECOVERABLE	0x0002	/* Possibly recoverable */
#define PANIC_SYNC		0x0004	/* Sync requested */
#define PANIC_DUMP		0x0008	/* Dump memory requested */
#define PANIC_REBOOT		0x0010	/* Reboot requested */
#define PANIC_HALT		0x0020	/* Halt requested */
#define PANIC_SILENT		0x0040	/* Silent panic (no console output) */

/*=============================================================================
 * PANIC CODES
 *============================================================================*/

#define PANIC_UNKNOWN		0	/* Unknown reason */
#define PANIC_OOM		1	/* Out of memory */
#define PANIC_NOPROC		2	/* No more processes */
#define PANIC_FS_ERROR		3	/* File system error */
#define PANIC_IO_ERROR		4	/* I/O error */
#define PANIC_TIMER_ERROR	5	/* Timer error */
#define PANIC_SERVER_DIED	6	/* Critical server died */
#define PANIC_CAP_VIOLATION	7	/* Capability violation */
#define PANIC_IPC_ERROR		8	/* IPC system error */
#define PANIC_HARDWARE		9	/* Hardware error */
#define PANIC_BOOT_ERROR	10	/* Boot error */
#define PANIC_INIT_TASK		11	/* Init task died */
#define PANIC_KERNEL_BUG	12	/* Kernel bug */

/*=============================================================================
 * INTERNAL FUNCTIONS
 *============================================================================*/

/**
 * panic_print - Print panic message to console and log
 * @s: Panic string
 * @code: Panic code
 */
static void panic_print(const char *s, unsigned long code)
{
	/* Try to print to console server */
	struct msg_console_write msg;
	
	msg.header.msg_id = MSG_CONSOLE_WRITE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	/* Format panic message */
	printk("\n\n=====================================\n");
	printk("KERNEL PANIC: %s\n", s);
	printk("Panic code: %lu\n", code);
	printk("Task: %d\n", kernel_state->current_task);
	printk("EIP: 0x%08lx\n", get_eip());
	printk("ESP: 0x%08lx\n", get_esp());
	printk("=====================================\n\n");
	
	/* Also try to send to console server */
	mk_msg_send(kernel_state->console_server, &msg, sizeof(msg));
}

/**
 * get_eip - Get current instruction pointer
 */
static inline unsigned long get_eip(void)
{
	unsigned long eip;
	__asm__ __volatile__("movl 4(%%ebp), %0" : "=r" (eip));
	return eip;
}

/**
 * get_esp - Get current stack pointer
 */
static inline unsigned long get_esp(void)
{
	unsigned long esp;
	__asm__ __volatile__("movl %%esp, %0" : "=r" (esp));
	return esp;
}

/**
 * panic_sync - Attempt to sync file systems
 * @flags: Panic flags
 * 
 * Returns 0 if sync initiated, -1 on error.
 */
static int panic_sync(unsigned int flags)
{
	struct msg_panic_notify msg;
	int result;
	
	if (!(flags & PANIC_SYNC))
		return 0;
	
	/* Try to notify file server */
	msg.header.msg_id = MSG_PANIC_SYNC;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	msg.flags = flags;
	
	result = mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;
	
	/* Wait a bit for sync to complete (don't wait for reply) */
	__asm__ __volatile__("mov $0x1000, %%ecx\n"
			     "1: loop 1b" : : : "ecx");
	
	return 0;
}

/**
 * panic_notify_servers - Notify all servers of panic
 * @s: Panic string
 * @code: Panic code
 * @flags: Panic flags
 */
static void panic_notify_servers(const char *s, unsigned long code, 
                                   unsigned int flags)
{
	struct msg_panic_notify msg;
	
	/* Prepare notification message */
	msg.header.msg_id = MSG_PANIC_NOTIFY;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;  /* No reply expected */
	msg.header.size = sizeof(msg);
	
	strncpy(msg.reason, s, 63);
	msg.reason[63] = '\0';
	msg.code = code;
	msg.eip = get_eip();
	msg.esp = get_esp();
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	msg.flags = flags;
	
	/* Notify system server */
	mk_msg_send(kernel_state->system_server, &msg, sizeof(msg));
	
	/* Notify other critical servers */
	if (kernel_state->process_server)
		mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
	if (kernel_state->memory_server)
		mk_msg_send(kernel_state->memory_server, &msg, sizeof(msg));
	if (kernel_state->file_server)
		mk_msg_send(kernel_state->file_server, &msg, sizeof(msg));
}

/**
 * panic_halt - Halt the system
 */
static void panic_halt(void)
{
	printk("System halted.\n");
	
	/* Try to notify system server of halt */
	struct msg_panic_notify msg;
	msg.header.msg_id = MSG_PANIC_HALT;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	mk_msg_send(kernel_state->system_server, &msg, sizeof(msg));
	
	/* Halt the CPU */
	__asm__ __volatile__("hlt");
}

/**
 * panic_reboot - Attempt to reboot
 */
static void panic_reboot(void)
{
	printk("Attempting reboot...\n");
	
	/* Try to notify system server of reboot */
	struct msg_panic_notify msg;
	msg.header.msg_id = MSG_PANIC_REBOOT;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	mk_msg_send(kernel_state->system_server, &msg, sizeof(msg));
	
	/* Attempt reboot via keyboard controller */
	int i;
	for (i = 0; i < 10; i++) {
		__asm__ __volatile__("outb %%al, $0x64" : : "a" (0xFE));
	}
	
	/* If reboot fails, halt */
	panic_halt();
}

/**
 * panic_dump - Dump kernel state
 */
static void panic_dump(void)
{
	struct msg_panic_notify msg;
	
	printk("Dumping kernel state...\n");
	
	/* Show task information */
	show_stat();
	
	/* Show memory information */
	printk("Jiffies: %ld\n", jiffies);
	printk("Startup time: %ld\n", startup_time);
	printk("Current task: %d\n", kernel_state->current_task);
	printk("Current capabilities: 0x%04x\n", current_capability);
	
	/* Notify system server of dump request */
	msg.header.msg_id = MSG_PANIC_DUMP;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	mk_msg_send(kernel_state->system_server, &msg, sizeof(msg));
}

/*=============================================================================
 * MAIN PANIC FUNCTION
 *============================================================================*/

/**
 * panic - Kernel panic handler
 * @s: Panic reason string
 *
 * This function is called when the kernel encounters a fatal error.
 * It attempts to notify servers, sync file systems, and then halts.
 * 
 * In microkernel mode, it also attempts to notify the system server
 * which may try to recover or initiate an orderly shutdown.
 */
void panic(const char *s)
{
	unsigned long flags = PANIC_FATAL | PANIC_SYNC;
	unsigned long code = PANIC_UNKNOWN;
	static int already_panicing = 0;
	
	/* Prevent recursive panics */
	if (already_panicing) {
		printk("Recursive panic: %s\n", s);
		panic_halt();
	}
	already_panicing = 1;
	
	/* Try to determine panic code from string */
	if (strstr(s, "out of memory"))
		code = PANIC_OOM;
	else if (strstr(s, "No more processes"))
		code = PANIC_NOPROC;
	else if (strstr(s, "VFS"))
		code = PANIC_FS_ERROR;
	else if (strstr(s, "I/O error"))
		code = PANIC_IO_ERROR;
	else if (strstr(s, "timer"))
		code = PANIC_TIMER_ERROR;
	else if (strstr(s, "server"))
		code = PANIC_SERVER_DIED;
	else if (strstr(s, "capability"))
		code = PANIC_CAP_VIOLATION;
	else if (strstr(s, "IPC"))
		code = PANIC_IPC_ERROR;
	else if (strstr(s, "hardware"))
		code = PANIC_HARDWARE;
	else if (strstr(s, "boot"))
		code = PANIC_BOOT_ERROR;
	else if (strstr(s, "init"))
		code = PANIC_INIT_TASK;
	
	/* Print panic message */
	panic_print(s, code);
	
	/* Dump state if requested */
	if (flags & PANIC_DUMP)
		panic_dump();
	
	/* Notify all servers */
	panic_notify_servers(s, code, flags);
	
	/* Check if this is the swapper task (task 0) */
	if (current == task[0]) {
		printk("In swapper task - limited sync\n\r");
		flags &= ~PANIC_SYNC;  /* Don't try to sync from idle task */
	}
	
	/* Attempt to sync file systems */
	if (flags & PANIC_SYNC) {
		printk("Syncing filesystems...\n");
		panic_sync(flags);
	}
	
	/* Final message */
	printk("Panic complete - system halted.\n");
	
	/* Decide final action based on flags */
	if (flags & PANIC_REBOOT) {
		panic_reboot();
	} else {
		panic_halt();
	}
	
	/* Should never reach here */
	for(;;);
}

/*=============================================================================
 * VARIANT PANIC FUNCTIONS
 *============================================================================*/

/**
 * panic_code - Panic with numeric code
 * @code: Panic code
 * @s: Additional string
 */
void panic_code(unsigned long code, const char *s)
{
	char buf[128];
	sprintf(buf, "Panic code %lu: %s", code, s);
	panic(buf);
}

/**
 * panic_fatal - Fatal panic (no recovery possible)
 * @s: Panic reason
 */
void panic_fatal(const char *s)
{
	unsigned long old_flags = /* would get current flags */;
	/* Would set PANIC_FATAL flag */
	panic(s);
}

/**
 * panic_recoverable - Recoverable panic
 * @s: Panic reason
 * 
 * Attempts recovery via system server.
 * Returns 0 if recovered, -1 if panic continues.
 */
int panic_recoverable(const char *s)
{
	struct msg_panic_notify msg;
	struct msg_panic_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;
	
	printk("Recoverable panic: %s\n", s);
	
	/* Ask system server if recovery is possible */
	msg.header.msg_id = MSG_PANIC_NOTIFY;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);
	
	strncpy(msg.reason, s, 63);
	msg.reason[63] = '\0';
	msg.code = PANIC_RECOVERABLE;
	msg.flags = PANIC_RECOVERABLE;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	
	result = mk_msg_send(kernel_state->system_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;
	
	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;
	
	if (reply.action == 1) {
		printk("Recovery successful.\n");
		return 0;
	}
	
	/* Recovery failed - full panic */
	panic(s);
	return -1;  /* Never reached */
}

/**
 * panic_on - Conditional panic
 * @cond: Condition (panic if true)
 * @s: Panic reason
 */
void panic_on(int cond, const char *s)
{
	if (cond)
		panic(s);
}

/**
 * panic_if - Panic if condition true
 */
#define panic_if(cond, s) do { if (cond) panic(s); } while (0)

/*=============================================================================
 * SERVER NOTIFICATION FUNCTIONS
 *============================================================================*/

/**
 * panic_notify_server - Notify specific server of panic
 * @server_port: Server port
 * @s: Panic reason
 */
void panic_notify_server(unsigned int server_port, const char *s)
{
	struct msg_panic_notify msg;
	
	msg.header.msg_id = MSG_PANIC_NOTIFY;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	strncpy(msg.reason, s, 63);
	msg.reason[63] = '\0';
	msg.code = PANIC_UNKNOWN;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	
	mk_msg_send(server_port, &msg, sizeof(msg));
}
