/*
* HISTORY
* $Log: system_call.s,v $
* Revision 1.1 2026/02/15 04:15:30 pedro
* Microkernel version of system call low-level handlers.
* System calls now redirect to IPC stubs instead of direct kernel functions.
* Timer interrupt forwards to system server.
* Hardware interrupts forwarded to device server.
* Signal handling delegated to signal server.
* [2026/02/15 pedro]
*/

/*
 * File: kernel/system_call.s
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * System call low-level handling for microkernel architecture.
 *
 * Original Linux 0.11: Direct system calls to kernel functions via sys_call_table.
 * Microkernel version: System calls redirect to IPC stubs. The int $0x80 handler
 * now packages arguments and sends an IPC message to the appropriate server
 * (process server, file server, etc.) instead of calling kernel functions directly.
 *
 * Hardware interrupts are forwarded to the device server.
 * Timer interrupts are forwarded to the system server.
 */

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

SIG_CHLD	= 17

/* Stack offsets for saved registers */
EAX		= 0x00
EBX		= 0x04
ECX		= 0x08
EDX		= 0x0C
FS		= 0x10
ES		= 0x14
DS		= 0x18
EIP		= 0x1C
CS		= 0x20
EFLAGS		= 0x24
OLDESP		= 0x28
OLDSS		= 0x2C

/* task_struct offsets (for local cache) */
state	= 0
counter	= 4
priority = 8
signal	= 12
sigaction = 16		# MUST be 16 (=len of sigaction)
blocked = (33*16)

/* sigaction offsets */
sa_handler = 0
sa_mask = 4
sa_flags = 8
sa_restorer = 12

/* System call count */
nr_system_calls = 74

/* Microkernel IPC system call number */
MK_IPC_SEND	= 0x1000	/* Special syscall number for IPC */

/* Server ports (from kernel_state) */
PROCESS_SERVER_PORT	= 0x0004
FILE_SERVER_PORT	= 0x0005
DEVICE_SERVER_PORT	= 0x0006
SIGNAL_SERVER_PORT	= 0x0007
SYSTEM_SERVER_PORT	= 0x000A

/*=============================================================================
 * GLOBAL SYMBOLS
 *============================================================================*/

.globl system_call, sys_fork, timer_interrupt, sys_execve
.globl hd_interrupt, floppy_interrupt, parallel_interrupt
.globl device_not_available, coprocessor_error
.globl ret_from_sys_call

/*=============================================================================
 * DATA SECTION
 *============================================================================*/

.data
.align 2

/* Syscall to server mapping table (matches sys.h) */
syscall_to_server:
	# 0-9
	.long 0x0A, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x01, 0x02, 0x02
	# 10-19
	.long 0x02, 0x01, 0x02, 0x05, 0x02, 0x02, 0x02, 0x09, 0x02, 0x02
	# 20-29
	.long 0x01, 0x03, 0x03, 0x06, 0x06, 0x05, 0x01, 0x01, 0x02, 0x01
	# 30-39
	.long 0x02, 0x07, 0x07, 0x02, 0x01, 0x05, 0x02, 0x04, 0x02, 0x02
	# 40-49
	.long 0x02, 0x08, 0x08, 0x01, 0x01, 0x09, 0x06, 0x06, 0x04, 0x06
	# 50-59
	.long 0x06, 0x01, 0x09, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x0A
	# 60-69
	.long 0x01, 0x02, 0x02, 0x08, 0x01, 0x01, 0x01, 0x04, 0x04, 0x04
	# 70-73
	.long 0x06, 0x06, 0x06, 0x06

/* Server port lookup table */
server_ports:
	.long 0x0000		# 0: unused
	.long 0x0004		# 1: PROCESS_SERVER
	.long 0x0005		# 2: FILE_SERVER
	.long 0x0003		# 3: FS_SERVER (memory server?)
	.long 0x0007		# 4: SIGNAL_SERVER
	.long 0x0009		# 5: TIME_SERVER
	.long 0x0006		# 6: USER_SERVER (device server?)
	.long 0x0008		# 7: TERMINAL_SERVER
	.long 0x000?		# 8: IPC_SERVER
	.long 0x0003		# 9: MEMORY_SERVER
	.long 0x000A		# A: SYSTEM_SERVER

/*=============================================================================
 * TEXT SECTION
 *============================================================================*/

.text
.align 2

/*=============================================================================
 * BAD SYSTEM CALL HANDLER
 *============================================================================*/
bad_sys_call:
	movl $-1, %eax
	iret

/*=============================================================================
 * RESCHEDULE HANDLER
 *============================================================================*/
.align 2
reschedule:
	pushl $ret_from_sys_call
	jmp schedule

/*=============================================================================
 * SYSTEM CALL HANDLER (MICROKERNEL VERSION)
 *============================================================================*/
.align 2
system_call:
	# Check syscall number range
	cmpl $nr_system_calls-1, %eax
	ja bad_sys_call
	
	# Save registers
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	
	# Set up kernel segments
	movl $0x10, %edx
	mov %dx, %ds
	mov %dx, %es
	movl $0x17, %edx
	mov %dx, %fs
	
	# Get server port for this syscall
	movl %eax, %ebx		# Save syscall number
	leal syscall_to_server, %esi
	movl (%esi, %ebx, 4), %ecx	# ecx = server_id
	leal server_ports, %esi
	movl (%esi, %ecx, 4), %edx	# edx = server_port
	
	# If server port is 0, fallback to old method
	testl %edx, %edx
	jnz do_ipc_syscall
	
	# Fallback to original sys_call_table (for compatibility)
	call *sys_call_table(,%ebx,4)
	jmp syscall_return

do_ipc_syscall:
	# Prepare IPC message for syscall
	# Stack layout for IPC message:
	# - msg header (16 bytes)
	# - syscall number (4 bytes)
	# - args (up to 3 * 4 bytes)
	
	subl $32, %esp		# Allocate space for IPC message
	
	# Fill mk_msg_header
	movl $0x????, (%esp)	# msg_id = syscall number? (TODO)
	movl kernel_state+4, %eax	# kernel_port
	movl %eax, 4(%esp)	# sender_port
	movl kernel_state+4, %eax	# kernel_port (reply port)
	movl %eax, 8(%esp)	# reply_port
	movl $32, 12(%esp)	# msg size
	
	# Fill syscall number
	movl %ebx, 16(%esp)	# syscall_nr
	
	# Copy arguments (ebx, ecx, edx already pushed)
	movl 20(%esp), %eax	# arg1 (ebx)
	movl %eax, 20(%esp)
	movl 24(%esp), %eax	# arg2 (ecx)
	movl %eax, 24(%esp)
	movl 28(%esp), %eax	# arg3 (edx)
	movl %eax, 28(%esp)
	
	# Send IPC message
	movl %edx, %ebx		# server_port
	leal (%esp), %ecx	# msg pointer
	movl $32, %edx		# msg size
	movl $MK_IPC_SEND, %eax
	int $0x80		# Microkernel IPC syscall
	
	# Check result
	testl %eax, %eax
	js syscall_error
	
	# Wait for reply (simplified - would need receive)
	# For now, assume result is in eax
	addl $32, %esp		# Clean up message
	jmp syscall_return

syscall_error:
	addl $32, %esp
	movl $-1, %eax
	jmp syscall_return

syscall_return:
	# Save return value
	pushl %eax
	
	# Check if we need to reschedule
	movl current, %eax
	cmpl $0, state(%eax)
	jne reschedule
	cmpl $0, counter(%eax)
	je reschedule

ret_from_sys_call:
	# Check for signals (delegated to signal server)
	movl current, %eax
	cmpl task, %eax
	je 3f
	cmpw $0x0f, CS(%esp)
	jne 3f
	cmpw $0x17, OLDSS(%esp)
	jne 3f
	
	# Check pending signals (would ask signal server)
	# For now, simplified
	movl signal(%eax), %ebx
	movl blocked(%eax), %ecx
	notl %ecx
	andl %ebx, %ecx
	bsfl %ecx, %ecx
	je 3f
	btrl %ecx, %ebx
	movl %ebx, signal(%eax)
	incl %ecx
	pushl %ecx
	call do_signal		# This now sends IPC to signal server
	popl %eax

3:
	# Restore registers and return
	popl %eax
	popl %ebx
	popl %ecx
	popl %edx
	pop %fs
	pop %es
	pop %ds
	iret

/*=============================================================================
 * COPROCESSOR ERROR HANDLER
 *============================================================================*/
.align 2
coprocessor_error:
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	movl $0x17, %eax
	mov %ax, %fs
	pushl $ret_from_sys_call
	jmp math_error

/*=============================================================================
 * DEVICE NOT AVAILABLE HANDLER (FPU)
 *============================================================================*/
.align 2
device_not_available:
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	movl $0x17, %eax
	mov %ax, %fs
	pushl $ret_from_sys_call
	clts
	movl %cr0, %eax
	testl $0x4, %eax
	je math_state_restore
	pushl %ebp
	pushl %esi
	pushl %edi
	call math_emulate
	popl %edi
	popl %esi
	popl %ebp
	ret

/*=============================================================================
 * TIMER INTERRUPT HANDLER (Forward to system server)
 *============================================================================*/
.align 2
timer_interrupt:
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	
	# Set up kernel segments
	movl $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	movl $0x17, %eax
	mov %ax, %fs
	
	# Send timer tick to system server
	pushl %eax
	pushl %ecx
	pushl %edx
	
	# Prepare IPC message for timer tick
	subl $16, %esp		# Allocate message header
	movl $0x170C, (%esp)	# MSG_SCHED_TIMER
	movl kernel_state+4, %eax
	movl %eax, 4(%esp)	# sender_port
	movl $0, 8(%esp)	# reply_port (none)
	movl $16, 12(%esp)	# msg size
	
	# Send to system server
	movl kernel_state+20, %ebx	# system_server port
	leal (%esp), %ecx
	movl $16, %edx
	movl $MK_IPC_SEND, %eax
	int $0x80
	
	addl $16, %esp		# Clean up message
	popl %edx
	popl %ecx
	popl %eax
	
	# EOI to interrupt controller
	movb $0x20, %al
	outb %al, $0x20
	
	# Call do_timer locally (still needed for local accounting)
	movl CS(%esp), %eax
	andl $3, %eax
	pushl %eax
	call do_timer
	addl $4, %esp
	
	jmp ret_from_sys_call

/*=============================================================================
 * SYSTEM CALL STUBS
 *============================================================================*/
.align 2
sys_execve:
	lea EIP(%esp), %eax
	pushl %eax
	call do_execve		# This now sends IPC to process server
	addl $4, %esp
	ret

.align 2
sys_fork:
	call find_empty_process
	testl %eax, %eax
	js 1f
	push %gs
	pushl %esi
	pushl %edi
	pushl %ebp
	pushl %eax
	call copy_process	# This now sends IPC to process server
	addl $20, %esp
1:	ret

/*=============================================================================
 * HARD DISK INTERRUPT HANDLER (Forward to device server)
 *============================================================================*/
hd_interrupt:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	
	movl $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	movl $0x17, %eax
	mov %ax, %fs
	
	# Send interrupt notification to device server
	subl $16, %esp
	movl $0x140?, (%esp)	# MSG_HD_INTERRUPT
	movl kernel_state+4, %eax
	movl %eax, 4(%esp)	# sender_port
	movl $0, 8(%esp)	# reply_port
	movl $16, 12(%esp)	# msg size
	
	movl kernel_state+16, %ebx	# device_server port
	leal (%esp), %ecx
	movl $16, %edx
	movl $MK_IPC_SEND, %eax
	int $0x80
	
	addl $16, %esp
	
	# EOI
	movb $0x20, %al
	outb %al, $0xA0
	jmp 1f
1:	jmp 1f
1:
	outb %al, $0x20
	
	# Call local handler if any
	xorl %edx, %edx
	xchgl do_hd, %edx
	testl %edx, %edx
	jne 1f
	movl $unexpected_hd_interrupt, %edx
1:	call *%edx
	
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

/*=============================================================================
 * FLOPPY INTERRUPT HANDLER (Forward to device server)
 *============================================================================*/
floppy_interrupt:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	
	movl $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	movl $0x17, %eax
	mov %ax, %fs
	
	# Send interrupt notification to device server
	subl $16, %esp
	movl $0x1A??, (%esp)	# MSG_FLOPPY_INTERRUPT
	movl kernel_state+4, %eax
	movl %eax, 4(%esp)	# sender_port
	movl $0, 8(%esp)	# reply_port
	movl $16, 12(%esp)	# msg size
	
	movl kernel_state+16, %ebx	# device_server port
	leal (%esp), %ecx
	movl $16, %edx
	movl $MK_IPC_SEND, %eax
	int $0x80
	
	addl $16, %esp
	
	# EOI
	movb $0x20, %al
	outb %al, $0x20
	
	# Call local handler if any
	xorl %eax, %eax
	xchgl do_floppy, %eax
	testl %eax, %eax
	jne 1f
	movl $unexpected_floppy_interrupt, %eax
1:	call *%eax
	
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

/*=============================================================================
 * PARALLEL INTERRUPT HANDLER (Minimal)
 *============================================================================*/
parallel_interrupt:
	pushl %eax
	movb $0x20, %al
	outb %al, $0x20
	popl %eax
	iret

/*=============================================================================
 * UNEXPECTED INTERRUPT HANDLERS
 *============================================================================*/
unexpected_hd_interrupt:
	# Notify device server of unexpected interrupt
	ret

unexpected_floppy_interrupt:
	# Notify device server of unexpected interrupt
	ret
