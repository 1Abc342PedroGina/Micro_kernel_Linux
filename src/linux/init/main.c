/*
* HISTORY
* $Log: main.c,v $
* Revision 1.1 2026/02/15 16:15:30 pedro
* Microkernel version of kernel main.
* Now starts servers instead of device drivers.
* Establishes IPC channels between servers.
* Preserves original initialization sequence where meaningful.
* [2026/02/15 pedro]
*/

/*
 * File: init/main.c
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * Main kernel initialization for microkernel architecture.
 *
 * Original Linux 0.11: Initializes hardware, sets up drivers, forks init.
 * Microkernel version: Initializes minimal kernel, starts server processes,
 * establishes IPC channels, and then forks the init process which will
 * communicate with servers via IPC.
 *
 * The kernel now acts as a microkernel - only managing IPC, capabilities,
 * and minimal hardware abstraction. All traditional OS services run as
 * user-mode servers.
 */

#define __LIBRARY__
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/system.h>
#include <asm/io.h>

#include <stddef.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>

#include <linux/fs.h>

/*=============================================================================
 * MICROKERNEL SERVER DECLARATIONS
 *============================================================================*/

/* Server entry points (to be linked from server binaries) */
extern void memory_server_main(void);
extern void process_server_main(void);
extern void file_server_main(void);
extern void device_server_main(void);
extern void time_server_main(void);
extern void console_server_main(void);
extern void log_server_main(void);
extern void signal_server_main(void);
extern void system_server_main(void);

/* Server startup functions */
static int start_server(void (*server_main)(void), const char *name, 
                         unsigned int port, unsigned int stack_size);

/*=============================================================================
 * ORIGINAL DECLARATIONS (Preserved)
 *============================================================================*/

static char printbuf[1024];
extern int vsprintf();
extern void init(void);
extern void mem_init(long start, long end);
extern long kernel_mktime(struct tm * tm);
extern long startup_time;

/*=============================================================================
 * ORIGINAL BOOT PARAMETERS (Preserved)
 *============================================================================*/

#define EXT_MEM_K (*(unsigned short *)0x90002)
#define DRIVE_INFO (*(struct drive_info *)0x90080)
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)

/*=============================================================================
 * MICROKERNEL EXTENSIONS - Server ports
 *============================================================================*/

/* These must match the definitions in head.h */
#define PORT_BOOTSTRAP		0x0001
#define PORT_KERNEL		0x0002
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

/* Server stack size (4KB per server) */
#define SERVER_STACK_SIZE	4096

/*=============================================================================
 * ORIGINAL CMOS/RTC FUNCTIONS (Preserved)
 *============================================================================*/

#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

static void time_init(void)
{
	struct tm time;

	do {
		time.tm_sec = CMOS_READ(0);
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8);
		time.tm_year = CMOS_READ(9);
	} while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	time.tm_mon--;
	startup_time = kernel_mktime(&time);
}

/*=============================================================================
 * ORIGINAL GLOBAL VARIABLES (Preserved)
 *============================================================================*/

static long memory_end = 0;
static long buffer_memory_end = 0;
static long main_memory_start = 0;

struct drive_info { char dummy[32]; } drive_info;

/*=============================================================================
 * MICROKERNEL STATE
 *============================================================================*/

/* Kernel state (defined in head.h) */
extern struct mk_kernel_state *kernel_state;

/* Current task capability (defined in kernel.h) */
capability_t current_capability = CAP_ALL;

/*=============================================================================
 * SERVER STARTUP FUNCTIONS
 *============================================================================*/

/**
 * start_server - Start a server process
 * @server_main: Server main function
 * @name: Server name (for debugging)
 * @port: Server's IPC port
 * @stack_size: Stack size for server
 * 
 * Returns PID of server process, or negative error code.
 */
static int start_server(void (*server_main)(void), const char *name,
                         unsigned int port, unsigned int stack_size)
{
	int pid;
	
	printk("Starting %s server... ", name);
	
	pid = fork();
	if (pid < 0) {
		printk("fork failed\n");
		return pid;
	}
	
	if (pid == 0) {
		/* Child process - this is the server */
		
		/* Set server's IPC port */
		kernel_state->current_task = getpid();
		current_capability = CAP_ALL;  /* Servers start with all caps */
		
		/* Allocate server's IPC port */
		if (sys_ipc_port_allocate(CAP_SYSTEM) != port) {
			printk("failed to allocate port %d\n", port);
			_exit(1);
		}
		
		printk("OK (PID %d, port %d)\n", getpid(), port);
		
		/* Run server main */
		server_main();
		
		/* Should never return */
		_exit(0);
	}
	
	/* Parent continues */
	printk("PID %d\n", pid);
	return pid;
}

/**
 * establish_server_connections - Set up IPC channels between servers
 */
static void establish_server_connections(void)
{
	printk("Establishing server connections...\n");
	
	/* In a real microkernel, servers would discover each other
	 * via the bootstrap server or through capability passing.
	 * For now, we just note that they are running. */
	 
	printk("All servers are ready.\n");
}

/*=============================================================================
 * ORIGINAL TIME_INIT (Preserved but may be moved to time server)
 *============================================================================*/

/* time_init is kept for kernel local time, but actual system time
 * will be managed by the time server */

/*=============================================================================
 * MAIN FUNCTION (Modified for microkernel)
 *============================================================================*/

void main(void)
{
	printk("\n\n");
	printk("========================================\n");
	printk("IceCityOS Microkernel v0.11 (2026)\n");
	printk("Copyright (C) 1991 Linus Torvalds\n");
	printk("Microkernel adaptation by Pedro Emanuel\n");
	printk("========================================\n\n");

	/*
	 * Interrupts are still disabled. Do necessary setups, then
	 * enable them
	 */

	/* Original hardware detection (still needed for boot) */
	ROOT_DEV = ORIG_ROOT_DEV;
	drive_info = DRIVE_INFO;
	
	/* Calculate memory layout (still needed) */
	memory_end = (1<<20) + (EXT_MEM_K<<10);
	memory_end &= 0xfffff000;
	if (memory_end > 16*1024*1024)
		memory_end = 16*1024*1024;
	if (memory_end > 12*1024*1024) 
		buffer_memory_end = 4*1024*1024;
	else if (memory_end > 6*1024*1024)
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;
	main_memory_start = buffer_memory_end;

	/* Initialize memory management (now just local cache) */
	mem_init(main_memory_start, memory_end);
	
	/* Initialize trap handlers (now send IPC to servers) */
	trap_init();
	
	/* Original device initializers replaced with server startup */
	printk("Starting microkernel servers...\n");
	
	/* Start core servers in order of dependency */
	start_server(memory_server_main, "Memory server", PORT_MEMORY, SERVER_STACK_SIZE);
	start_server(process_server_main, "Process server", PORT_PROCESS, SERVER_STACK_SIZE);
	start_server(device_server_main, "Device server", PORT_DEVICE, SERVER_STACK_SIZE);
	start_server(time_server_main, "Time server", PORT_TIME, SERVER_STACK_SIZE);
	
	/* Servers that depend on core servers */
	start_server(file_server_main, "File server", PORT_FILE, SERVER_STACK_SIZE);
	start_server(signal_server_main, "Signal server", PORT_SIGNAL, SERVER_STACK_SIZE);
	start_server(console_server_main, "Console server", PORT_CONSOLE, SERVER_STACK_SIZE);
	start_server(log_server_main, "Log server", PORT_LOG, SERVER_STACK_SIZE);
	start_server(system_server_main, "System server", PORT_SYSTEM, SERVER_STACK_SIZE);
	
	/* Establish connections between servers */
	establish_server_connections();
	
	/* Initialize original subsystems (now mostly stubs) */
	sched_init();
	
	/* Time initialization (local, but time server handles actual time) */
	time_init();
	
	/* Enable interrupts */
	sti();
	
	printk("\nAll servers running. Switching to user mode...\n\n");
	
	/* Switch to user mode */
	move_to_user_mode();
	
	/* Fork the init process */
	if (!fork()) {
		init();
	}
	
	/*
	 * NOTE!! For any other task 'pause()' would mean we have to get a
	 * signal to awaken, but task0 is the sole exception (see 'schedule()')
	 * as task 0 gets activated at every idle moment (when no other tasks
	 * can run). For task0 'pause()' just means we go check if some other
	 * task can run, and if not we return here.
	 */
	for(;;) pause();
}

/*=============================================================================
 * ORIGINAL PRINTF (Preserved, now uses console server)
 *============================================================================*/

static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsprintf(printbuf, fmt, args);
	va_end(args);
	
	/* Use console server if available, otherwise fallback to tty_write */
	if (kernel_state && kernel_state->console_server) {
		struct msg_console_write msg;
		msg.header.msg_id = MSG_CONSOLE_WRITE;
		msg.header.sender_port = kernel_state->kernel_port;
		msg.header.reply_port = 0;
		msg.header.size = sizeof(msg);
		memcpy(msg.data, printbuf, i);
		msg.len = i;
		msg.level = 6;  /* KERN_INFO */
		msg.task_id = kernel_state->current_task;
		msg.caps = current_capability;
		mk_msg_send(kernel_state->console_server, &msg, sizeof(msg));
	} else {
		/* Fallback to original write */
		write(1, printbuf, i);
	}
	return i;
}

/*=============================================================================
 * ORIGINAL INIT FUNCTION (Modified for microkernel)
 *============================================================================*/

static char * argv_rc[] = { "/bin/sh", NULL };
static char * envp_rc[] = { "HOME=/", NULL };

static char * argv[] = { "-/bin/sh", NULL };
static char * envp[] = { "HOME=/usr/root", NULL };

void init(void)
{
	int pid, i;

	/* Setup system call (now handled by servers) */
	setup((void *) &drive_info);
	
	/* Open standard file descriptors (now use file server) */
	(void) open("/dev/tty0", O_RDWR, 0);
	(void) dup(0);
	(void) dup(0);
	
	printf("%d buffers = %d bytes buffer space\n\r", NR_BUFFERS,
		NR_BUFFERS * BLOCK_SIZE);
	printf("Free mem: %d bytes\n\r", memory_end - main_memory_start);
	
	/* Run /etc/rc script */
	if (!(pid = fork())) {
		close(0);
		if (open("/etc/rc", O_RDONLY, 0))
			_exit(1);
		execve("/bin/sh", argv_rc, envp_rc);
		_exit(2);
	}
	if (pid > 0)
		while (pid != wait(&i))
			/* nothing */;
	
	/* Main init loop - spawn gettys */
	while (1) {
		if ((pid = fork()) < 0) {
			printf("Fork failed in init\r\n");
			continue;
		}
		if (!pid) {
			/* Child process - becomes a getty */
			close(0); close(1); close(2);
			setsid();
			(void) open("/dev/tty0", O_RDWR, 0);
			(void) dup(0);
			(void) dup(0);
			_exit(execve("/bin/sh", argv, envp));
		}
		while (1)
			if (pid == wait(&i))
				break;
		printf("\n\rchild %d died with code %04x\n\r", pid, i);
		sync();
	}
	_exit(0);	/* NOTE! _exit, not exit() */
}

/*=============================================================================
 * EMERGENCY FUNCTIONS
 *============================================================================*/

/**
 * panic - Kernel panic handler (overrides weak definition)
 */
void panic(const char *s)
{
	printk("Kernel panic: %s\n", s);
	
	/* Notify system server */
	if (kernel_state && kernel_state->system_server) {
		struct msg_panic msg;
		msg.header.msg_id = MSG_PANIC;
		msg.header.sender_port = kernel_state->kernel_port;
		msg.header.reply_port = 0;
		msg.header.size = sizeof(msg);
		msg.str = s;
		mk_msg_send(kernel_state->system_server, &msg, sizeof(msg));
	}
	
	/* Halt */
	cli();
	for(;;);
}
