/*
* HISTORY
* $Log: system.h,v $
* Revision 1.1 2026/02/13 22:45:30 pedro
* Microkernel version of system operations.
* move_to_user_mode now switches capability spaces.
* cli/sti become capability-controlled operations.
* Gate descriptors replaced with capability-based IPC handlers.
* TSS/LDT descriptors now represent capability contexts.
* [2026/02/13 pedro]
*/

/*
* File: asm/system.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/13
*
* System operations for microkernel architecture.
*
* Original Linux 0.11: Direct x86 system operations (cli, sti, iret),
* interrupt gate setup, TSS/LDT descriptors, and user mode switching.
*
* Microkernel version: All privileged operations are delegated to
* the system server via IPC. cli/sti become capability checks.
* Gates become IPC ports for exception handling.
* TSS/LDT become capability contexts for tasks.
*/

#ifndef _ASM_SYSTEM_H
#define _ASM_SYSTEM_H

#include <linux/kernel.h>
#include <linux/head.h>

/* Message codes for system operations */
#define MSG_SYS_MOVE_TO_USER	0x0800	/* Switch to user mode */
#define MSG_SYS_CLI		0x0801	/* Disable interrupts (capability check) */
#define MSG_SYS_STI		0x0802	/* Enable interrupts (capability check) */
#define MSG_SYS_IRET		0x0803	/* Return from interrupt */
#define MSG_SYS_SET_GATE	0x0804	/* Set IPC gate for exception */
#define MSG_SYS_SET_INTR_GATE	0x0805	/* Set interrupt gate */
#define MSG_SYS_SET_TRAP_GATE	0x0806	/* Set trap gate */
#define MSG_SYS_SET_SYSTEM_GATE	0x0807	/* Set system gate */
#define MSG_SYS_SET_TSS		0x0808	/* Set TSS descriptor */
#define MSG_SYS_SET_LDT		0x0809	/* Set LDT descriptor */
#define MSG_SYS_EXCEPTION	0x080A	/* Exception occurred */

/* System operation message structures */
struct msg_sys_move_user {
	struct mk_msg_header header;
	unsigned int task_id;		/* Task to switch to user mode */
	unsigned long esp;		/* User stack pointer */
	unsigned long eip;		/* User entry point */
	capability_t caps;		/* Caller capabilities */
	unsigned int user_space;	/* User capability space ID */
};

struct msg_sys_cli_sti {
	struct mk_msg_header header;
	unsigned int task_id;		/* Task requesting operation */
	capability_t caps;		/* Caller capabilities */
	unsigned int cpu_id;		/* CPU ID (for SMP) */
};

struct msg_sys_set_gate {
	struct mk_msg_header header;
	unsigned int gate_idx;		/* Gate index (0-255) */
	unsigned int gate_type;		/* Gate type (intr/trap/system) */
	unsigned int dpl;		/* Descriptor privilege level */
	unsigned int handler_port;	/* IPC port for handler */
	unsigned int task_id;		/* Task owning the gate */
	capability_t caps;		/* Caller capabilities */
};

struct msg_sys_set_tss_ldt {
	struct mk_msg_header header;
	unsigned int idx;		/* Descriptor index */
	unsigned long addr;		/* Base address */
	unsigned int type;		/* TSS (0x89) or LDT (0x82) */
	unsigned int task_id;		/* Task owning the descriptor */
	capability_t caps;		/* Caller capabilities */
};

struct msg_sys_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	unsigned long value;		/* Return value */
};

/* Gate types (compatible with x86 but now abstract) */
#define GATE_TYPE_INTR	14	/* Interrupt gate */
#define GATE_TYPE_TRAP	15	/* Trap gate */
#define GATE_TYPE_SYSTEM	15	/* System gate (with DPL=3) */

/* Descriptor types */
#define DESC_TSS	0x89
#define DESC_LDT	0x82

/* Capability required for system operations */
#define CAP_SYSTEM	0x0020	/* System capability (from kernel.h) */

/* Current CPU state (simplified for microkernel) */
extern unsigned int current_cpu;
extern unsigned int interrupts_enabled;

/**
 * move_to_user_mode - Switch current task to user mode
 *
 * Transitions the current task from kernel mode to user mode.
 * In microkernel architecture, this involves:
 * 1. Switching from kernel capability space to user space
 * 2. Setting up user stack and instruction pointer
 * 3. Notifying system server of mode change
 *
 * The original x86 iret mechanism is now handled by the system server.
 */
#define move_to_user_mode() \
do { \
	struct msg_sys_move_user msg; \
	struct msg_sys_reply reply; \
	unsigned int reply_size = sizeof(reply); \
	\
	/* Check system capability */ \
	if (!(current_capability & CAP_SYSTEM)) { \
		/* Cannot switch to user mode without capability */ \
		break; \
	} \
	\
	/* Prepare message */ \
	msg.header.msg_id = MSG_SYS_MOVE_TO_USER; \
	msg.header.sender_port = kernel_state->kernel_port; \
	msg.header.reply_port = kernel_state->kernel_port; \
	msg.header.size = sizeof(msg); \
	\
	/* Get current stack and instruction pointers */ \
	__asm__ __volatile__ ( \
		"movl %%esp, %0\n\t" \
		"movl $1f, %1\n" \
		"1:" \
		: "=r" (msg.esp), "=r" (msg.eip) \
	); \
	\
	msg.task_id = kernel_state->current_task; \
	msg.caps = current_capability; \
	msg.user_space = SPACE_USER; /* User capability space */ \
	\
	/* Send to system server and wait for confirmation */ \
	if (mk_msg_send(kernel_state->system_server, &msg, sizeof(msg)) == 0) { \
		mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size); \
	} \
	\
	/* If we return here, we're back in kernel mode */ \
} while (0)

/**
 * sti - Enable interrupts (if permitted)
 *
 * Enables interrupts for the current CPU. In microkernel mode,
 * this checks if the task has capability to control interrupts.
 * The actual interrupt enable is performed by the system server.
 */
#define sti() \
do { \
	struct msg_sys_cli_sti msg; \
	\
	if (current_capability & CAP_SYSTEM) { \
		msg.header.msg_id = MSG_SYS_STI; \
		msg.header.sender_port = kernel_state->kernel_port; \
		msg.header.reply_port = 0; \
		msg.header.size = sizeof(msg); \
		\
		msg.task_id = kernel_state->current_task; \
		msg.caps = current_capability; \
		msg.cpu_id = current_cpu; \
		\
		mk_msg_send(kernel_state->system_server, &msg, sizeof(msg)); \
		interrupts_enabled = 1; \
	} \
} while (0)

/**
 * cli - Disable interrupts (if permitted)
 *
 * Disables interrupts for the current CPU. Only tasks with
 * CAP_SYSTEM can disable interrupts.
 */
#define cli() \
do { \
	struct msg_sys_cli_sti msg; \
	\
	if (current_capability & CAP_SYSTEM) { \
		msg.header.msg_id = MSG_SYS_CLI; \
		msg.header.sender_port = kernel_state->kernel_port; \
		msg.header.reply_port = 0; \
		msg.header.size = sizeof(msg); \
		\
		msg.task_id = kernel_state->current_task; \
		msg.caps = current_capability; \
		msg.cpu_id = current_cpu; \
		\
		mk_msg_send(kernel_state->system_server, &msg, sizeof(msg)); \
		interrupts_enabled = 0; \
	} \
} while (0)

/**
 * nop - No operation
 *
 * Still a real nop instruction - doesn't require privileges.
 */
#define nop() __asm__ __volatile__ ("nop"::)

/**
 * iret - Return from interrupt
 *
 * In microkernel mode, iret is handled by the system server
 * which restores the capability context and returns to user mode.
 */
#define iret() \
do { \
	struct msg_sys_iret msg; \
	\
	msg.header.msg_id = MSG_SYS_IRET; \
	msg.header.sender_port = kernel_state->kernel_port; \
	msg.header.reply_port = 0; /* Never returns */ \
	msg.header.size = sizeof(msg); \
	\
	msg.task_id = kernel_state->current_task; \
	msg.caps = current_capability; \
	\
	mk_msg_send(kernel_state->system_server, &msg, sizeof(msg)); \
	/* Should never reach here */ \
	for(;;); \
} while (0)

/**
 * set_intr_gate - Set interrupt gate
 * @n: Gate index (0-255)
 * @handler_port: IPC port for interrupt handler
 *
 * Sets up an interrupt gate that redirects to an IPC port.
 * When interrupt occurs, system server sends message to handler port.
 */
#define set_intr_gate(n, handler_port) \
	_set_gate(n, GATE_TYPE_INTR, 0, handler_port)

/**
 * set_trap_gate - Set trap gate
 * @n: Gate index (0-255)
 * @handler_port: IPC port for trap handler
 */
#define set_trap_gate(n, handler_port) \
	_set_gate(n, GATE_TYPE_TRAP, 0, handler_port)

/**
 * set_system_gate - Set system gate (accessible from user mode)
 * @n: Gate index (0-255)
 * @handler_port: IPC port for system call handler
 */
#define set_system_gate(n, handler_port) \
	_set_gate(n, GATE_TYPE_SYSTEM, 3, handler_port)

/**
 * _set_gate - Internal function to set a gate
 * @gate_idx: Gate index
 * @type: Gate type
 * @dpl: Descriptor privilege level
 * @handler_port: IPC port for handler
 */
static inline void _set_gate(unsigned int gate_idx, unsigned int type, 
			      unsigned int dpl, unsigned int handler_port)
{
	struct msg_sys_set_gate msg;
	
	if (!(current_capability & CAP_SYSTEM))
		return;
	
	msg.header.msg_id = MSG_SYS_SET_GATE;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.gate_idx = gate_idx;
	msg.gate_type = type;
	msg.dpl = dpl;
	msg.handler_port = handler_port;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	
	mk_msg_send(kernel_state->system_server, &msg, sizeof(msg));
}

/**
 * set_tss_desc - Set Task State Segment descriptor
 * @n: Descriptor index
 * @addr: Address of TSS structure
 *
 * In microkernel mode, TSS represents a capability context
 * for a task, including its IPC ports and memory spaces.
 */
#define set_tss_desc(n, addr) \
	_set_tss_ldt_desc(n, (unsigned long)(addr), DESC_TSS)

/**
 * set_ldt_desc - Set Local Descriptor Table descriptor
 * @n: Descriptor index
 * @addr: Address of LDT structure
 *
 * LDT now represents a task's local capability space.
 */
#define set_ldt_desc(n, addr) \
	_set_tss_ldt_desc(n, (unsigned long)(addr), DESC_LDT)

/**
 * _set_tss_ldt_desc - Internal function to set TSS/LDT descriptors
 * @n: Descriptor index
 * @addr: Base address
 * @type: DESC_TSS or DESC_LDT
 */
static inline void _set_tss_ldt_desc(unsigned int n, unsigned long addr, 
				       unsigned int type)
{
	struct msg_sys_set_tss_ldt msg;
	
	if (!(current_capability & CAP_SYSTEM))
		return;
	
	msg.header.msg_id = MSG_SYS_SET_TSS;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = 0;
	msg.header.size = sizeof(msg);
	
	msg.idx = n;
	msg.addr = addr;
	msg.type = type;
	msg.task_id = kernel_state->current_task;
	msg.caps = current_capability;
	
	mk_msg_send(kernel_state->system_server, &msg, sizeof(msg));
}

/**
 * save_flags - Save interrupt flags
 * @flags: Variable to store flags
 *
 * Returns current interrupt state (from local copy).
 */
#define save_flags(flags) \
	((flags) = interrupts_enabled ? 0x200 : 0)

/**
 * restore_flags - Restore interrupt flags
 * @flags: Flags to restore
 *
 * Restores interrupt state based on flags.
 */
#define restore_flags(flags) \
do { \
	if (flags & 0x200) \
		sti(); \
	else \
		cli(); \
} while (0)

/**
 * local_irq_disable - Disable interrupts locally
 */
#define local_irq_disable() cli()

/**
 * local_irq_enable - Enable interrupts locally
 */
#define local_irq_enable() sti()

/**
 * local_save_flags - Save local interrupt flags
 */
#define local_save_flags(flags) save_flags(flags)

/**
 * local_irq_restore - Restore local interrupt flags
 */
#define local_irq_restore(flags) restore_flags(flags)

/**
 * irq_enabled - Check if interrupts are enabled
 */
static inline int irq_enabled(void)
{
	return interrupts_enabled;
}

/**
 * register_exception_handler - Register handler for exception
 * @exception_nr: Exception number (0-31)
 * @handler_port: IPC port to receive exception messages
 *
 * Tasks can register to handle specific exceptions via IPC.
 */
static inline void register_exception_handler(unsigned int exception_nr,
					       unsigned int handler_port)
{
	_set_gate(exception_nr, GATE_TYPE_TRAP, 0, handler_port);
}

#endif /* _ASM_SYSTEM_H */
