/*
* HISTORY
* $Log: asm.s,v $
* Revision 1.1 2026/02/15 08:15:30 pedro
* Microkernel version of low-level hardware fault handlers.
* All exceptions now send IPC messages to process server.
* Preserved original stack frame layout for compatibility.
* Added microkernel-specific fault handling path.
* [2026/02/15 pedro]
*/

/*
 * File: kernel/asm.s
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * Low-level hardware fault handlers for microkernel architecture.
 *
 * Original Linux 0.11: Direct calls to C handlers (do_divide_error, etc.)
 * Microkernel version: Exceptions now send IPC messages to the process server,
 * which then delivers appropriate signals to the faulting task. The C handlers
 * are still called as fallback if the server is unavailable.
 *
 * The stack layout is preserved to maintain compatibility with the original
 * C handlers, but a new path is added for IPC-based exception handling.
 */

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

/* Microkernel IPC system call number */
MK_IPC_SEND	= 0x1000

/* Server ports (from kernel_state) */
PROCESS_SERVER_PORT	= 0x0004

/* Message IDs for exceptions (matches traps.c) */
MSG_TRAP_DIVIDE		= 0x1900
MSG_TRAP_DEBUG		= 0x1901
MSG_TRAP_NMI		= 0x1902
MSG_TRAP_INT3		= 0x1903
MSG_TRAP_OVERFLOW	= 0x1904
MSG_TRAP_BOUNDS		= 0x1905
MSG_TRAP_INVALID_OP	= 0x1906
MSG_TRAP_DEVICE_NA	= 0x1907
MSG_TRAP_DOUBLE_FAULT	= 0x1908
MSG_TRAP_COPROC_SEG	= 0x1909
MSG_TRAP_INVALID_TSS	= 0x190A
MSG_TRAP_SEG_NOT_PRES	= 0x190B
MSG_TRAP_STACK_SEG	= 0x190C
MSG_TRAP_GPF		= 0x190D
MSG_TRAP_PAGE_FAULT	= 0x190E
MSG_TRAP_RESERVED	= 0x190F
MSG_TRAP_COPROC_ERROR	= 0x1910

/*=============================================================================
 * GLOBAL SYMBOLS
 *============================================================================*/

.globl divide_error, debug, nmi, int3, overflow, bounds, invalid_op
.globl double_fault, coprocessor_segment_overrun
.globl invalid_TSS, segment_not_present, stack_segment
.globl general_protection, coprocessor_error, irq13, reserved

/*=============================================================================
 * TEXT SECTION
 *============================================================================*/

.text
.align 2

/*=============================================================================
 * MICROKERNEL EXCEPTION HANDLING MACRO
 *============================================================================*/

/*
 * This macro sends an IPC message to the process server with exception details.
 * It preserves all registers and stack state for potential fallback.
 */
.macro SEND_EXCEPTION_IPC msg_id
	# Save all registers (same as original no_error_code path)
	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi
	pushl %ebp
	push %ds
	push %es
	push %fs
	
	# Set up kernel segments
	movl $0x10, %edx
	mov %dx, %ds
	mov %dx, %es
	mov %dx, %fs
	
	# Allocate space for IPC message (32 bytes header + 32 bytes data)
	subl $64, %esp
	
	# Fill mk_msg_header
	movl $\msg_id, (%esp)		# msg_id
	movl kernel_state+4, %eax	# kernel_port
	movl %eax, 4(%esp)		# sender_port
	movl kernel_state+4, %eax	# kernel_port (reply port)
	movl %eax, 8(%esp)		# reply_port
	movl $64, 12(%esp)		# msg size
	
	# Fill exception data
	movl 64+44(%esp), %eax		# original eip (from stack)
	movl %eax, 16(%esp)		# eip
	movl 64+48(%esp), %eax		# original cs
	movl %eax, 20(%esp)		# cs
	movl 64+52(%esp), %eax		# original eflags
	movl %eax, 24(%esp)		# eflags
	movl kernel_state+8, %eax	# current_task
	movl %eax, 28(%esp)		# task_id
	movl current_capability, %eax
	movl %eax, 32(%esp)		# caps
	
	# Send IPC message
	movl $PROCESS_SERVER_PORT, %ebx
	leal (%esp), %ecx
	movl $64, %edx
	movl $MK_IPC_SEND, %eax
	int $0x80
	
	# Check result
	testl %eax, %eax
	js 1f				# If error, fall back to original handler
	
	# Clean up and return (exception handled by server)
	addl $64, %esp
	pop %fs
	pop %es
	pop %ds
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret
	
1:
	# Server unavailable - fall back to original C handler
	addl $64, %esp
	# Restore original handler address from stack
	movl 0(%esp), %eax		# Handler address pushed by caller
	jmp 2f
.endm

/*=============================================================================
 * ORIGINAL HANDLER MACROS (Preserved for fallback)
 *============================================================================*/

/*
 * These macros are the original no_error_code and error_code handlers,
 * preserved exactly for fallback operation.
 */

.macro ORIGINAL_NO_ERROR_CODE handler
	pushl $\handler
	xchgl %eax,(%esp)
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi
	pushl %ebp
	push %ds
	push %es
	push %fs
	pushl $0		# "error code"
	lea 44(%esp),%edx
	pushl %edx
	movl $0x10,%edx
	mov %dx,%ds
	mov %dx,%es
	mov %dx,%fs
	call *%eax
	addl $8,%esp
	pop %fs
	pop %es
	pop %ds
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret
.endm

.macro ORIGINAL_ERROR_CODE handler
	pushl $\handler
	xchgl %eax,4(%esp)		# error code <-> %eax
	xchgl %ebx,(%esp)		# &function <-> %ebx
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi
	pushl %ebp
	push %ds
	push %es
	push %fs
	pushl %eax			# error code
	lea 44(%esp),%eax		# offset
	pushl %eax
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	call *%ebx
	addl $8,%esp
	pop %fs
	pop %es
	pop %ds
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret
.endm

/*=============================================================================
 * EXCEPTION HANDLERS (Microkernel version)
 *============================================================================*/

/*
 * Each handler first attempts to send an IPC to the process server.
 * If that fails, it falls back to the original C handler.
 */

.align 2
divide_error:
	SEND_EXCEPTION_IPC MSG_TRAP_DIVIDE
2:	ORIGINAL_NO_ERROR_CODE do_divide_error

.align 2
debug:
	SEND_EXCEPTION_IPC MSG_TRAP_DEBUG
2:	ORIGINAL_NO_ERROR_CODE do_debug

.align 2
nmi:
	SEND_EXCEPTION_IPC MSG_TRAP_NMI
2:	ORIGINAL_NO_ERROR_CODE do_nmi

.align 2
int3:
	SEND_EXCEPTION_IPC MSG_TRAP_INT3
2:	ORIGINAL_NO_ERROR_CODE do_int3

.align 2
overflow:
	SEND_EXCEPTION_IPC MSG_TRAP_OVERFLOW
2:	ORIGINAL_NO_ERROR_CODE do_overflow

.align 2
bounds:
	SEND_EXCEPTION_IPC MSG_TRAP_BOUNDS
2:	ORIGINAL_NO_ERROR_CODE do_bounds

.align 2
invalid_op:
	SEND_EXCEPTION_IPC MSG_TRAP_INVALID_OP
2:	ORIGINAL_NO_ERROR_CODE do_invalid_op

.align 2
coprocessor_segment_overrun:
	SEND_EXCEPTION_IPC MSG_TRAP_COPROC_SEG
2:	ORIGINAL_NO_ERROR_CODE do_coprocessor_segment_overrun

.align 2
reserved:
	SEND_EXCEPTION_IPC MSG_TRAP_RESERVED
2:	ORIGINAL_NO_ERROR_CODE do_reserved

.align 2
double_fault:
	# Double fault is fatal - try IPC but likely won't work
	SEND_EXCEPTION_IPC MSG_TRAP_DOUBLE_FAULT
2:	ORIGINAL_ERROR_CODE do_double_fault

.align 2
invalid_TSS:
	SEND_EXCEPTION_IPC MSG_TRAP_INVALID_TSS
2:	ORIGINAL_ERROR_CODE do_invalid_TSS

.align 2
segment_not_present:
	SEND_EXCEPTION_IPC MSG_TRAP_SEG_NOT_PRES
2:	ORIGINAL_ERROR_CODE do_segment_not_present

.align 2
stack_segment:
	SEND_EXCEPTION_IPC MSG_TRAP_STACK_SEG
2:	ORIGINAL_ERROR_CODE do_stack_segment

.align 2
general_protection:
	SEND_EXCEPTION_IPC MSG_TRAP_GPF
2:	ORIGINAL_ERROR_CODE do_general_protection

/*=============================================================================
 * COPROCESSOR ERROR HANDLER
 *============================================================================*/

.align 2
coprocessor_error:
	# Special handling for coprocessor errors
	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi
	pushl %ebp
	push %ds
	push %es
	push %fs
	
	movl $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	
	# Check if this is our task
	movl last_task_used_math, %eax
	cmpl current, %eax
	je 1f
	
	# Not our task - just return
	pop %fs
	pop %es
	pop %ds
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret
	
1:
	# Our task - send IPC
	SEND_EXCEPTION_IPC MSG_TRAP_COPROC_ERROR
2:	ORIGINAL_NO_ERROR_CODE do_coprocessor_error

/*=============================================================================
 * IRQ13 HANDLER (Coprocessor interrupt)
 *============================================================================*/

.align 2
irq13:
	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	
	# Clear coprocessor busy
	xorb %al, %al
	outb %al, $0xF0
	
	# EOI to interrupt controllers
	movb $0x20, %al
	outb %al, $0x20
	jmp 1f
1:	jmp 1f
1:	outb %al, $0xA0
	
	# Send IPC to device server (simplified)
	subl $16, %esp
	movl $0x1911, (%esp)		# MSG_TRAP_IRQ13
	movl kernel_state+4, %eax
	movl %eax, 4(%esp)
	movl $0, 8(%esp)
	movl $16, 12(%esp)
	
	movl $DEVICE_SERVER_PORT, %ebx
	leal (%esp), %ecx
	movl $16, %edx
	movl $MK_IPC_SEND, %eax
	int $0x80
	
	addl $16, %esp
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	
	# Jump to original coprocessor_error handler
	jmp coprocessor_error

/*=============================================================================
 * UNUSED HANDLER (for completeness)
 *============================================================================*/

# This was in the original but not used
# page_exception is handled in mm, not here
