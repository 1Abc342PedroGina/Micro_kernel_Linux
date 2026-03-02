/*
* HISTORY
* $Log: traps.c,v $
* Revision 1.1 2026/02/14 22:30:45 pedro
* Microkernel version of trap handlers.
* Hardware exceptions now send IPC messages to process server.
* Exceptions are converted to signals for the offending task.
* Die function notifies system server before killing task.
* [2026/02/14 pedro]
*/

/*
 * File: kernel/traps.c
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/14
 *
 * Trap and exception handling for microkernel architecture.
 *
 * Original Linux 0.11: Direct hardware trap handlers that dump debug info
 * and kill the offending process.
 *
 * Microkernel version: Trap handlers send IPC messages to the process server,
 * which then delivers appropriate signals to the task. The kernel maintains
 * minimal handlers that capture state and forward to servers.
 *
 * Security: Exception handling requires CAP_PROCESS capability. The process
 * server validates that the exception is properly handled.
 */

#include <string.h>
#include <signal.h>

#include <linux/head.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/io.h>

/*=============================================================================
 * ORIGINAL MACROS (Preserved for compatibility, now use IPC)
 *============================================================================*/

#define get_seg_byte(seg,addr) ({ \
register char __res; \
__asm__("push %%fs;mov %%ax,%%fs;movb %%fs:%2,%%al;pop %%fs" \
	:"=a" (__res):"0" (seg),"m" (*(addr))); \
__res;})

#define get_seg_long(seg,addr) ({ \
register unsigned long __res; \
__asm__("push %%fs;mov %%ax,%%fs;movl %%fs:%2,%%eax;pop %%fs" \
	:"=a" (__res):"0" (seg),"m" (*(addr))); \
__res;})

#define _fs() ({ \
register unsigned short __res; \
__asm__("mov %%fs,%%ax":"=a" (__res):); \
__res;})

/*=============================================================================
 * MICROKERNEL IPC MESSAGE CODES
 *============================================================================*/

#define MSG_TRAP_DIVIDE		0x1900	/* Divide error */
#define MSG_TRAP_DEBUG		0x1901	/* Debug exception */
#define MSG_TRAP_NMI		0x1902	/* Non-maskable interrupt */
#define MSG_TRAP_INT3		0x1903	/* Breakpoint */
#define MSG_TRAP_OVERFLOW	0x1904	/* Overflow */
#define MSG_TRAP_BOUNDS		0x1905	/* Bounds check */
#define MSG_TRAP_INVALID_OP	0x1906	/* Invalid opcode */
#define MSG_TRAP_DEVICE_NA	0x1907	/* Device not available */
#define MSG_TRAP_DOUBLE_FAULT	0x1908	/* Double fault */
#define MSG_TRAP_COPROC_SEG	0x1909	/* Coprocessor segment overrun */
#define MSG_TRAP_INVALID_TSS	0x190A	/* Invalid TSS */
#define MSG_TRAP_SEG_NOT_PRES	0x190B	/* Segment not present */
#define MSG_TRAP_STACK_SEG	0x190C	/* Stack segment */
#define MSG_TRAP_GPF		0x190D	/* General protection fault */
#define MSG_TRAP_PAGE_FAULT	0x190E	/* Page fault */
#define MSG_TRAP_RESERVED	0x190F	/* Reserved exception */
#define MSG_TRAP_COPROC_ERROR	0x1910	/* Coprocessor error */
#define MSG_TRAP_IRQ13		0x1911	/* IRQ13 (coprocessor) */
#define MSG_TRAP_PARALLEL	0x1912	/* Parallel interrupt */
#define MSG_TRAP_DIE		0x1913	/* Die handler */
#define MSG_TRAP_REPLY		0x1914	/* Reply from process server */

/*=============================================================================
 * IPC MESSAGE STRUCTURES
 *============================================================================*/

struct msg_trap {
	struct mk_msg_header header;
	unsigned long esp;		/* Stack pointer at exception */
	unsigned long error_code;	/* Error code from CPU */
	unsigned long eip;		/* Instruction pointer */
	unsigned long cs;		/* Code segment */
	unsigned long eflags;		/* Flags */
	unsigned long cr2;		/* CR2 register (for page faults) */
	unsigned int task_id;		/* Task that caused exception */
	capability_t caps;		/* Caller capabilities */
	
	/* Additional register state */
	unsigned long eax, ebx, ecx, edx;
	unsigned long esi, edi, ebp;
	unsigned long ds, es, fs, gs;
};

struct msg_trap_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	int signal;			/* Signal to deliver */
	unsigned long action;		/* Action to take */
};

/*=============================================================================
 * CAPABILITY FLAGS
 *============================================================================*/

#define CAP_TRAP_HANDLE	0x0100	/* Can handle traps */

/*=============================================================================
 * EXCEPTION TO SIGNAL MAPPING
 *============================================================================*/

static const int trap_signal[] = {
	[SIGFPE]	= SIGFPE,	/* Divide error */
	[SIGTRAP]	= SIGTRAP,	/* Debug */
	[SIGTRAP]	= SIGTRAP,	/* NMI */
	[SIGTRAP]	= SIGTRAP,	/* INT3 */
	[SIGFPE]	= SIGFPE,	/* Overflow */
	[SIGFPE]	= SIGFPE,	/* Bounds */
	[SIGILL]	= SIGILL,	/* Invalid op */
	[SIGSEGV]	= SIGSEGV,	/* Device not available */
	[SIGSEGV]	= SIGSEGV,	/* Double fault */
	[SIGSEGV]	= SIGSEGV,	/* Coprocessor segment */
	[SIGSEGV]	= SIGSEGV,	/* Invalid TSS */
	[SIGSEGV]	= SIGSEGV,	/* Segment not present */
	[SIGSEGV]	= SIGSEGV,	/* Stack segment */
	[SIGSEGV]	= SIGSEGV,	/* General protection */
	[SIGSEGV]	= SIGSEGV,	/* Page fault */
	[SIGSEGV]	= SIGSEGV,	/* Reserved */
	[SIGFPE]	= SIGFPE,	/* Coprocessor error */
};

/*=============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

int do_exit(long code);
void page_exception(void);

/*=============================================================================
 * TRAP HANDLERS (Original prototypes)
 *============================================================================*/

void divide_error(void);
void debug(void);
void nmi(void);
void int3(void);
void overflow(void);
void bounds(void);
void invalid_op(void);
void device_not_available(void);
void double_fault(void);
void coprocessor_segment_overrun(void);
void invalid_TSS(void);
void segment_not_present(void);
void stack_segment(void);
void general_protection(void);
void page_fault(void);
void coprocessor_error(void);
void reserved(void);
void parallel_interrupt(void);
void irq13(void);

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * send_trap_message - Send trap information to process server
 * @msg_id: Message ID (trap type)
 * @esp: Stack pointer
 * @error_code: Error code
 * @signal: Suggested signal
 * 
 * Returns 0 if handled, -1 if process server unavailable.
 */
static int send_trap_message(unsigned int msg_id, long esp, long error_code,
                              int *signal)
{
	struct msg_trap msg;
	struct msg_trap_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	/* Fill trap message */
	msg.header.msg_id = msg_id;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	/* Get current register state */
	__asm__ __volatile__(
		"movl %%eax, %0\n"
		"movl %%ebx, %1\n"
		"movl %%ecx, %2\n"
		"movl %%edx, %3\n"
		"movl %%esi, %4\n"
		"movl %%edi, %5\n"
		"movl %%ebp, %6\n"
		"movw %%ds, %7\n"
		"movw %%es, %8\n"
		"movw %%fs, %9\n"
		"movw %%gs, %10\n"
		: "=m" (msg.eax), "=m" (msg.ebx), "=m" (msg.ecx),
		  "=m" (msg.edx), "=m" (msg.esi), "=m" (msg.edi),
		  "=m" (msg.ebp), "=m" (msg.ds), "=m" (msg.es),
		  "=m" (msg.fs), "=m" (msg.gs)
		: 
		: "memory"
	);

	/* Get exception location */
	__asm__ __volatile__(
		"movl 4(%%ebp), %0\n"	/* Return address */
		: "=r" (msg.eip)
	);

	msg.esp = esp;
	msg.error_code = error_code;
	msg.cr2 = 0;
	
	/* Read CR2 for page faults */
	if (msg_id == MSG_TRAP_PAGE_FAULT) {
		__asm__("movl %%cr2, %0" : "=r" (msg.cr2));
	}

	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;

	/* Send to process server */
	result = mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
	if (result < 0) {
		printk("Trap: cannot notify process server\n");
		return -1;
	}

	/* Wait for reply */
	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0) {
		printk("Trap: no reply from process server\n");
		return -1;
	}

	if (signal)
		*signal = reply.signal;

	return reply.result;
}

/**
 * die - Print diagnostic information and kill task
 * @str: Error string
 * @esp_ptr: Stack pointer
 * @nr: Error number
 */
static void die(char *str, long esp_ptr, long nr)
{
	long *esp = (long *)esp_ptr;
	int i;

	/* Try to notify system server */
	struct msg_trap msg;
	msg.header.msg_id = MSG_TRAP_DIE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	mk_msg_send(kernel_state->system_server, &msg, sizeof(msg));

	/* Print diagnostic information */
	printk("\n***** KERNEL TRAP *****\n");
	printk("%s: %04x\n\r", str, nr & 0xffff);
	printk("EIP:\t%04x:%p\nEFLAGS:\t%p\nESP:\t%04x:%p\n",
		esp[1], esp[0], esp[2], esp[4], esp[3]);
	printk("fs: %04x\n", _fs());
	printk("base: %p, limit: %p\n", get_base(current->ldt[1]), 
	       get_limit(0x17));
	
	if (esp[4] == 0x17) {
		printk("Stack: ");
		for (i = 0; i < 4; i++)
			printk("%p ", get_seg_long(0x17, i + (long *)esp[3]));
		printk("\n");
	}

	printk("Pid: %d\n\r", current->pid);
	
	for (i = 0; i < 10; i++)
		printk("%02x ", 0xff & get_seg_byte(esp[1], 
		       (i + (char *)esp[0])));
	printk("\n\r**********************\n");

	/* Kill the task with appropriate signal */
	do_exit(11);  /* SIGSEGV */
}

/*=============================================================================
 * TRAP HANDLER IMPLEMENTATIONS
 *============================================================================*/

void do_divide_error(long esp, long error_code)
{
	int signal = SIGFPE;
	
	if (send_trap_message(MSG_TRAP_DIVIDE, esp, error_code, &signal) < 0)
		die("divide error", esp, error_code);
	
	/* If we return, the process server has handled it */
	current->signal |= (1 << (signal - 1));
}

void do_debug(long esp, long error_code)
{
	int signal = SIGTRAP;
	
	if (send_trap_message(MSG_TRAP_DEBUG, esp, error_code, &signal) < 0)
		die("debug", esp, error_code);
	
	current->signal |= (1 << (signal - 1));
}

void do_nmi(long esp, long error_code)
{
	/* NMI is usually fatal - always die */
	die("nmi", esp, error_code);
}

void do_int3(long *esp, long error_code,
	     long fs, long es, long ds,
	     long ebp, long esi, long edi,
	     long edx, long ecx, long ebx, long eax)
{
	int signal = SIGTRAP;
	int tr;

	/* Get task register */
	__asm__("str %%ax" : "=a" (tr) : "0" (0));

	/* Try to notify process server first */
	if (send_trap_message(MSG_TRAP_INT3, (long)esp, error_code, &signal) < 0) {
		/* Fallback to diagnostic output */
		printk("eax\t\tebx\t\tecx\t\tedx\n\r%8x\t%8x\t%8x\t%8x\n\r",
			eax, ebx, ecx, edx);
		printk("esi\t\tedi\t\tebp\t\tesp\n\r%8x\t%8x\t%8x\t%8x\n\r",
			esi, edi, ebp, (long)esp);
		printk("\n\rds\tes\tfs\ttr\n\r%4x\t%4x\t%4x\t%4x\n\r",
			ds, es, fs, tr);
		printk("EIP: %8x   CS: %4x  EFLAGS: %8x\n\r", 
		       esp[0], esp[1], esp[2]);
	}
	
	current->signal |= (1 << (signal - 1));
}

void do_overflow(long esp, long error_code)
{
	int signal = SIGFPE;
	
	if (send_trap_message(MSG_TRAP_OVERFLOW, esp, error_code, &signal) < 0)
		die("overflow", esp, error_code);
	
	current->signal |= (1 << (signal - 1));
}

void do_bounds(long esp, long error_code)
{
	int signal = SIGFPE;
	
	if (send_trap_message(MSG_TRAP_BOUNDS, esp, error_code, &signal) < 0)
		die("bounds", esp, error_code);
	
	current->signal |= (1 << (signal - 1));
}

void do_invalid_op(long esp, long error_code)
{
	int signal = SIGILL;
	
	if (send_trap_message(MSG_TRAP_INVALID_OP, esp, error_code, &signal) < 0)
		die("invalid operand", esp, error_code);
	
	current->signal |= (1 << (signal - 1));
}

void do_device_not_available(long esp, long error_code)
{
	int signal = SIGSEGV;
	
	/* Check if this is for FPU context switch */
	if (last_task_used_math != current) {
		math_state_restore();
		return;
	}
	
	if (send_trap_message(MSG_TRAP_DEVICE_NA, esp, error_code, &signal) < 0)
		die("device not available", esp, error_code);
	
	current->signal |= (1 << (signal - 1));
}

void do_double_fault(long esp, long error_code)
{
	/* Double fault is fatal - always die */
	die("double fault", esp, error_code);
}

void do_coprocessor_segment_overrun(long esp, long error_code)
{
	int signal = SIGSEGV;
	
	if (send_trap_message(MSG_TRAP_COPROC_SEG, esp, error_code, &signal) < 0)
		die("coprocessor segment overrun", esp, error_code);
	
	current->signal |= (1 << (signal - 1));
}

void do_invalid_TSS(long esp, long error_code)
{
	int signal = SIGSEGV;
	
	if (send_trap_message(MSG_TRAP_INVALID_TSS, esp, error_code, &signal) < 0)
		die("invalid TSS", esp, error_code);
	
	current->signal |= (1 << (signal - 1));
}

void do_segment_not_present(long esp, long error_code)
{
	int signal = SIGSEGV;
	
	if (send_trap_message(MSG_TRAP_SEG_NOT_PRES, esp, error_code, &signal) < 0)
		die("segment not present", esp, error_code);
	
	current->signal |= (1 << (signal - 1));
}

void do_stack_segment(long esp, long error_code)
{
	int signal = SIGSEGV;
	
	if (send_trap_message(MSG_TRAP_STACK_SEG, esp, error_code, &signal) < 0)
		die("stack segment", esp, error_code);
	
	current->signal |= (1 << (signal - 1));
}

void do_general_protection(long esp, long error_code)
{
	int signal = SIGSEGV;
	
	if (send_trap_message(MSG_TRAP_GPF, esp, error_code, &signal) < 0)
		die("general protection", esp, error_code);
	
	current->signal |= (1 << (signal - 1));
}

void do_page_fault(long esp, long error_code)
{
	int signal = SIGSEGV;
	unsigned long cr2;
	
	__asm__("movl %%cr2, %0" : "=r" (cr2));
	
	/* Page faults can be handled by memory server */
	if (send_trap_message(MSG_TRAP_PAGE_FAULT, esp, error_code, &signal) < 0) {
		printk("Page fault at %p, error code %ld\n", cr2, error_code);
		die("page fault", esp, error_code);
	}
	
	current->signal |= (1 << (signal - 1));
}

void do_coprocessor_error(long esp, long error_code)
{
	int signal = SIGFPE;
	
	if (last_task_used_math != current)
		return;
	
	if (send_trap_message(MSG_TRAP_COPROC_ERROR, esp, error_code, &signal) < 0)
		die("coprocessor error", esp, error_code);
	
	current->signal |= (1 << (signal - 1));
}

void do_reserved(long esp, long error_code)
{
	int signal = SIGSEGV;
	
	if (send_trap_message(MSG_TRAP_RESERVED, esp, error_code, &signal) < 0)
		die("reserved (15,17-47) error", esp, error_code);
	
	current->signal |= (1 << (signal - 1));
}

/*=============================================================================
 * INTERRUPT HANDLERS (IRQ)
 *============================================================================*/

void irq13(void)
{
	/* Coprocessor interrupt */
	outb_p(0, 0xF0);  /* Clear coprocessor busy */
	__asm__("fnclex");  /* Clear FPU exceptions */
	
	/* Notify process server if needed */
	struct msg_trap msg;
	msg.header.msg_id = MSG_TRAP_IRQ13;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
}

void parallel_interrupt(void)
{
	/* Parallel port interrupt - mostly ignored */
	struct msg_trap msg;
	msg.header.msg_id = MSG_TRAP_PARALLEL;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
}

/*=============================================================================
 * INITIALIZATION
 *============================================================================*/

void trap_init(void)
{
	int i;

	printk("Initializing trap handlers (microkernel mode)...\n");

	/* Set up exception gates */
	set_trap_gate(0, &divide_error);
	set_trap_gate(1, &debug);
	set_trap_gate(2, &nmi);
	set_system_gate(3, &int3);	/* int3-5 can be called from all */
	set_system_gate(4, &overflow);
	set_system_gate(5, &bounds);
	set_trap_gate(6, &invalid_op);
	set_trap_gate(7, &device_not_available);
	set_trap_gate(8, &double_fault);
	set_trap_gate(9, &coprocessor_segment_overrun);
	set_trap_gate(10, &invalid_TSS);
	set_trap_gate(11, &segment_not_present);
	set_trap_gate(12, &stack_segment);
	set_trap_gate(13, &general_protection);
	set_trap_gate(14, &page_fault);
	set_trap_gate(15, &reserved);
	set_trap_gate(16, &coprocessor_error);
	
	/* Fill reserved vectors */
	for (i = 17; i < 48; i++)
		set_trap_gate(i, &reserved);
	
	/* Set up hardware interrupt gates */
	set_trap_gate(45, &irq13);
	outb_p(inb_p(0x21) & 0xfb, 0x21);
	outb(inb_p(0xA1) & 0xdf, 0xA1);
	set_trap_gate(39, &parallel_interrupt);

	printk("Trap handlers initialized.\n");
}
