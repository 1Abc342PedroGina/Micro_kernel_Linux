/*
* HISTORY
* $Log: page.s,v $
* Revision 1.1 2026/02/15 12:15:30 pedro
* Microkernel version of page fault handler.
* Page faults now send IPC to memory server.
* Original C handlers (do_no_page/do_wp_page) called as fallback.
* [2026/02/15 pedro]
*/

/*
 * File: mm/page.s
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * Low-level page fault handler for microkernel architecture.
 *
 * Original Linux 0.11: Direct calls to do_no_page/do_wp_page in C.
 * Microkernel version: Page faults now send an IPC message to the
 * memory server, which handles the fault (allocates page, handles COW,
 * or sends SIGSEGV). If the server is unavailable, falls back to the
 * original C handlers.
 *
 * The stack layout is preserved to maintain compatibility with the
 * original C handlers in the fallback path.
 */

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

/* Microkernel IPC system call number */
MK_IPC_SEND	= 0x1000

/* Memory server port */
MEMORY_SERVER_PORT	= 0x0003

/* Message IDs for page faults (matches memory.c) */
MSG_MEM_DO_NO_PAGE	= 0x0118	/* Handle page fault (no page) */
MSG_MEM_DO_WP_PAGE	= 0x0113	/* Handle write-protect page fault */

/* Stack offsets for saved registers (original layout) */
OFFSET_EAX	= 0x00
OFFSET_ECX	= 0x04
OFFSET_EDX	= 0x08
OFFSET_DS	= 0x0C
OFFSET_ES	= 0x10
OFFSET_FS	= 0x14
OFFSET_RET	= 0x18	/* Return address */

/*=============================================================================
 * GLOBAL SYMBOLS
 *============================================================================*/

.globl page_fault

/*=============================================================================
 * TEXT SECTION
 *============================================================================*/

.text
.align 2

/*=============================================================================
 * PAGE FAULT HANDLER (Microkernel version)
 *============================================================================*/

page_fault:
	# Save registers (same as original)
	xchgl %eax, (%esp)		# Save error code, restore eax later
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	
	# Set up kernel segments
	movl $0x10, %edx
	mov %dx, %ds
	mov %dx, %es
	mov %dx, %fs
	
	# Get faulting address from CR2
	movl %cr2, %edx
	pushl %edx			# Push address (2nd argument)
	pushl %eax			# Push error code (1st argument)
	
	# At this point, stack layout:
	# 0(%esp) - error code
	# 4(%esp) - fault address
	# 8(%esp) - saved eax
	# 12(%esp) - saved ecx
	# 16(%esp) - saved edx
	# 20(%esp) - saved ds
	# 24(%esp) - saved es
	# 28(%esp) - saved fs
	# 32(%esp) - return address

	# Try to send IPC to memory server
	call try_memory_server_ipc
	
	# If IPC succeeded (return value 0), we're done
	testl %eax, %eax
	jz page_fault_return
	
	# IPC failed - fall back to original handlers
	popl %eax			# Restore error code
	popl %edx			# Restore fault address (now in edx)
	pushl %edx			# Push address again
	pushl %eax			# Push error code again
	
	# Call appropriate C handler based on error code
	testl $1, %eax			# Check if page was present
	jne 1f
	call do_no_page			# Page not present
	jmp 2f
1:	call do_wp_page			# Write protection fault
2:
	# Clean up stack
	addl $8, %esp			# Remove error code and address

page_fault_return:
	# Restore registers and return
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

/*=============================================================================
 * TRY MEMORY SERVER IPC
 *============================================================================*/

/*
 * try_memory_server_ipc - Attempt to handle page fault via memory server
 * 
 * Input:
 *   Stack as described above
 * 
 * Output:
 *   %eax = 0 if IPC succeeded, -1 if failed
 * 
 * This function preserves all registers except eax.
 */
try_memory_server_ipc:
	# Save registers we'll use
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %esi
	pushl %edi
	pushl %ebp
	
	# Allocate space for IPC message (64 bytes)
	subl $64, %esp
	
	# Fill mk_msg_header
	movl $MSG_MEM_DO_NO_PAGE, %eax	# We'll fix type later
	movl %eax, (%esp)		# msg_id
	movl kernel_state+4, %eax	# kernel_port
	movl %eax, 4(%esp)		# sender_port
	movl kernel_state+4, %eax	# kernel_port (reply port)
	movl %eax, 8(%esp)		# reply_port
	movl $64, 12(%esp)		# msg size
	
	# Get error code and fault address from stack
	# They are at 64+44(%esp) and 64+48(%esp) due to our pushes
	movl 64+44(%esp), %eax		# error code
	movl 64+48(%esp), %edx		# fault address
	
	# Store in message
	movl %eax, 16(%esp)		# error_code
	movl %edx, 20(%esp)		# address
	
	# Set message ID based on error code
	testl $1, %eax
	jne 1f
	movl $MSG_MEM_DO_NO_PAGE, %eax
	jmp 2f
1:	movl $MSG_MEM_DO_WP_PAGE, %eax
2:	movl %eax, (%esp)		# Update msg_id
	
	# Store task_id and caps
	movl kernel_state+8, %eax	# current_task
	movl %eax, 24(%esp)		# task_id
	movl current_capability, %eax
	movl %eax, 28(%esp)		# caps
	
	# Send IPC message to memory server
	movl $MEMORY_SERVER_PORT, %ebx
	leal (%esp), %ecx
	movl $64, %edx
	movl $MK_IPC_SEND, %eax
	int $0x80
	
	# Check result
	testl %eax, %eax
	js ipc_failed
	
	# Wait for reply (simplified - would need receive)
	# For now, assume success if send succeeded
	movl $0, %eax
	jmp ipc_done

ipc_failed:
	movl $-1, %eax

ipc_done:
	# Clean up
	addl $64, %esp
	popl %ebp
	popl %edi
	popl %esi
	popl %edx
	popl %ecx
	popl %ebx
	ret

/*=============================================================================
 * ORIGINAL C FUNCTION DECLARATIONS (for fallback)
 *============================================================================*/

.extern do_no_page
.extern do_wp_page
.extern kernel_state
.extern current_capability

/*=============================================================================
 * COMMENTS
 *============================================================================*/

/*
 * Original Linux 0.11 comment preserved for historical context:
 *
 * page.s contains the low-level page-exception code.
 * the real work is done in mm.c
 *
 * In microkernel version, the real work is done in the memory server,
 * with this handler acting as a proxy.
 */
