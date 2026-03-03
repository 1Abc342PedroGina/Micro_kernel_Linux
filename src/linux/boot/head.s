/*
* HISTORY
* $Log: head.s,v $
* Revision 1.1 2026/02/15 13:15:30 pedro
* Microkernel version of boot head.
* Extended to initialize capability tables.
* Added early IPC capability.
* Preserved original paging setup.
* [2026/02/15 pedro]
*/

/*
 * File: boot/head.s
 * Author: Linus Torvalds (original Linux version)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * 32-bit startup code for microkernel architecture.
 *
 * Original Linux 0.11: Sets up IDT, GDT, paging, and jumps to main().
 * Microkernel version: Extended to initialize capability structures,
 * set up initial IPC ports, and prepare for server startup. The kernel
 * now boots into a minimal environment that can communicate with the
 * bootstrap server.
 *
 * NOTE!!! Startup happens at absolute address 0x00000000, which is also where
 * the page directory will exist. The startup code will be overwritten by
 * the page directory.
 */

.text
.globl idt, gdt, pg_dir, tmp_floppy_area
.globl capability_table, server_ports, kernel_state

pg_dir:

/*=============================================================================
 * MICROKERNEL EXTENSIONS
 *============================================================================*/

/* Kernel state structure (defined in head.h) */
#define KERNEL_STATE_SIZE	64	/* Size in bytes */
#define OFFSET_KERNEL_PORT	4	/* Offset of kernel_port */
#define OFFSET_BOOTSTRAP_PORT	8	/* Offset of bootstrap_port */
#define OFFSET_MEMORY_SERVER	12	/* Offset of memory_server */
#define OFFSET_CONSOLE_SERVER	16	/* Offset of console_server */
#define OFFSET_LOG_SERVER	20	/* Offset of log_server */
#define OFFSET_PROCESS_SERVER	24	/* Offset of process_server */
#define OFFSET_SYSTEM_SERVER	28	/* Offset of system_server */
#define OFFSET_PANIC_CALLED	32	/* Offset of panic_called */
#define OFFSET_KERNEL_CAPS	36	/* Offset of kernel_caps */

/* Capability table */
#define CAP_TABLE_SIZE		256	/* 256 capability entries */
#define CAP_TABLE_ENTRY_SIZE	8	/* Each entry 8 bytes */

/* Server port definitions */
#define PORT_BOOTSTRAP		0x0001
#define PORT_KERNEL		0x0002
#define PORT_MEMORY		0x0003
#define PORT_CONSOLE		0x0004
#define PORT_LOG		0x0005
#define PORT_PROCESS		0x0006
#define PORT_SYSTEM		0x0007

/*=============================================================================
 * ORIGINAL STARTUP CODE (Modified)
 *============================================================================*/

.globl startup_32
startup_32:
	movl $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	lss stack_start, %esp
	
	/* Initialize microkernel extensions first */
	call setup_capability_table
	call setup_kernel_state
	call setup_server_ports
	
	/* Original initialization */
	call setup_idt
	call setup_gdt
	
	movl $0x10, %eax		# reload all the segment registers
	mov %ax, %ds			# after changing gdt. CS was already
	mov %ax, %es			# reloaded in 'setup_gdt'
	mov %ax, %fs
	mov %ax, %gs
	lss stack_start, %esp
	
	/* Check A20 line */
	xorl %eax, %eax
1:	incl %eax			# check that A20 really IS enabled
	movl %eax, 0x000000		# loop forever if it isn't
	cmpl %eax, 0x100000
	je 1b

	/* Setup cr0 */
	movl %cr0, %eax			# check math chip
	andl $0x80000011, %eax		# Save PG, PE, ET
	orl $2, %eax			# set MP
	movl %eax, %cr0
	call check_x87
	jmp after_page_tables

/*=============================================================================
 * ORIGINAL CHECK_X87 (Preserved)
 *============================================================================*/

/*
 * We depend on ET to be correct. This checks for 287/387.
 */
check_x87:
	fninit
	fstsw %ax
	cmpb $0, %al
	je 1f				/* no coprocessor: have to set bits */
	movl %cr0, %eax
	xorl $6, %eax			/* reset MP, set EM */
	movl %eax, %cr0
	ret
.align 2
1:	.byte 0xDB, 0xE4			/* fsetpm for 287, ignored by 387 */
	ret

/*=============================================================================
 * ORIGINAL SETUP_IDT (Preserved)
 *============================================================================*/

/*
 *  setup_idt
 *
 *  sets up a idt with 256 entries pointing to
 *  ignore_int, interrupt gates. It then loads
 *  idt. Everything that wants to install itself
 *  in the idt-table may do so themselves. Interrupts
 *  are enabled elsewhere, when we can be relatively
 *  sure everything is ok. This routine will be over-
 *  written by the page tables.
 */
setup_idt:
	lea ignore_int, %edx
	movl $0x00080000, %eax
	movw %dx, %ax			/* selector = 0x0008 = cs */
	movw $0x8E00, %dx		/* interrupt gate - dpl=0, present */

	lea idt, %edi
	mov $256, %ecx
rp_sidt:
	movl %eax, (%edi)
	movl %edx, 4(%edi)
	addl $8, %edi
	dec %ecx
	jne rp_sidt
	lidt idt_descr
	ret

/*=============================================================================
 * ORIGINAL SETUP_GDT (Preserved)
 *============================================================================*/

/*
 *  setup_gdt
 *
 *  This routines sets up a new gdt and loads it.
 *  Only two entries are currently built, the same
 *  ones that were built in init.s. The routine
 *  is VERY complicated at two whole lines, so this
 *  rather long comment is certainly needed :-).
 *  This routine will beoverwritten by the page tables.
 */
setup_gdt:
	lgdt gdt_descr
	ret

/*=============================================================================
 * MICROKERNEL INITIALIZATION ROUTINES
 *============================================================================*/

/*
 * setup_capability_table - Initialize capability structures
 */
setup_capability_table:
	# capability_table is at a fixed location after pg_dir
	leal capability_table, %edi
	movl $CAP_TABLE_SIZE, %ecx
	xorl %eax, %eax
	cld
	rep stosl			# Clear capability table
	
	# Set up kernel capability (all rights)
	leal capability_table, %edi
	movl $0xFFFFFFFF, 0(%edi)	# object_id (kernel)
	movl $0x0000FFFF, 4(%edi)	# rights (all)
	movl $0, 8(%edi)		# task (kernel)
	ret

/*
 * setup_kernel_state - Initialize kernel state structure
 */
setup_kernel_state:
	leal kernel_state, %edi
	movl $KERNEL_STATE_SIZE, %ecx
	xorl %eax, %eax
	cld
	rep stosb			# Clear kernel state
	
	# Set kernel port
	leal kernel_state, %edi
	movl $PORT_KERNEL, OFFSET_KERNEL_PORT(%edi)
	
	# Set bootstrap port
	movl $PORT_BOOTSTRAP, OFFSET_BOOTSTRAP_PORT(%edi)
	
	# Set kernel capabilities (all)
	movl $0xFFFF, OFFSET_KERNEL_CAPS(%edi)
	
	# Panic flag initially 0
	movl $0, OFFSET_PANIC_CALLED(%edi)
	ret

/*
 * setup_server_ports - Initialize well-known server ports
 */
setup_server_ports:
	leal kernel_state, %edi
	
	# Set server ports (will be connected during bootstrap)
	movl $PORT_MEMORY, OFFSET_MEMORY_SERVER(%edi)
	movl $PORT_CONSOLE, OFFSET_CONSOLE_SERVER(%edi)
	movl $PORT_LOG, OFFSET_LOG_SERVER(%edi)
	movl $PORT_PROCESS, OFFSET_PROCESS_SERVER(%edi)
	movl $PORT_SYSTEM, OFFSET_SYSTEM_SERVER(%edi)
	ret

/*=============================================================================
 * ORIGINAL PAGE TABLE AREAS (Preserved)
 *============================================================================*/

/*
 * I put the kernel page tables right after the page directory,
 * using 4 of them to span 16 Mb of physical memory. People with
 * more than 16MB will have to expand this.
 */
.org 0x1000
pg0:

.org 0x2000
pg1:

.org 0x3000
pg2:

.org 0x4000
pg3:

.org 0x5000
/*
 * tmp_floppy_area is used by the floppy-driver when DMA cannot
 * reach to a buffer-block. It needs to be aligned, so that it isn't
 * on a 64kB border.
 */
tmp_floppy_area:
	.fill 1024, 1, 0

/*=============================================================================
 * MICROKERNEL DATA AREAS
 *============================================================================*/

.org 0x6000
capability_table:
	.fill CAP_TABLE_SIZE * CAP_TABLE_ENTRY_SIZE, 1, 0

.org 0x7000
kernel_state:
	.fill KERNEL_STATE_SIZE, 1, 0

.org 0x8000
server_ports:
	.fill 256, 1, 0

/*=============================================================================
 * ORIGINAL AFTER_PAGE_TABLES (Modified)
 *============================================================================*/

after_page_tables:
	# Push parameters for main() - now includes kernel_state
	pushl $kernel_state		# kernel_state pointer
	pushl $0			# argc = 0
	pushl $0			# argv = NULL
	pushl $0			# envp = NULL
	pushl $L6			# return address for main
	pushl $main
	jmp setup_paging
L6:
	jmp L6				# main should never return here

/*=============================================================================
 * ORIGINAL INTERRUPT HANDLER (Preserved, but now prints via early printk)
 *============================================================================*/

/* This is the default interrupt "handler" :-) */
int_msg:
	.asciz "Unknown interrupt\n\r"
.align 2
ignore_int:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	pushl $int_msg
	call early_printk		# Use early_printk before servers ready
	popl %eax
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

/*=============================================================================
 * ORIGINAL SETUP_PAGING (Preserved)
 *============================================================================*/

/*
 * Setup_paging
 *
 * This routine sets up paging by setting the page bit
 * in cr0. The page tables are set up, identity-mapping
 * the first 16MB. The pager assumes that no illegal
 * addresses are produced (ie >4Mb on a 4Mb machine).
 *
 * NOTE! Although all physical memory should be identity
 * mapped by this routine, only the kernel page functions
 * use the >1Mb addresses directly. All "normal" functions
 * use just the lower 1Mb, or the local data space, which
 * will be mapped to some other place - mm keeps track of
 * that.
 *
 * For those with more memory than 16 Mb - tough luck. I've
 * not got it, why should you :-) The source is here. Change
 * it. (Seriously - it shouldn't be too difficult. Mostly
 * change some constants etc. I left it at 16Mb, as my machine
 * even cannot be extended past that (ok, but it was cheap :-)
 * I've tried to show which constants to change by having
 * some kind of marker at them (search for "16Mb"), but I
 * won't guarantee that's all :-( )
 */
.align 2
setup_paging:
	movl $1024*5, %ecx		/* 5 pages - pg_dir+4 page tables */
	xorl %eax, %eax
	xorl %edi, %edi			/* pg_dir is at 0x000 */
	cld; rep; stosl
	movl $pg0+7, pg_dir		/* set present bit/user r/w */
	movl $pg1+7, pg_dir+4		/*  --------- " " --------- */
	movl $pg2+7, pg_dir+8		/*  --------- " " --------- */
	movl $pg3+7, pg_dir+12		/*  --------- " " --------- */
	movl $pg3+4092, %edi
	movl $0xfff007, %eax		/*  16Mb - 4096 + 7 (r/w user,p) */
	std
1:	stosl				/* fill pages backwards - more efficient :-) */
	subl $0x1000, %eax
	jge 1b
	cld
	xorl %eax, %eax			/* pg_dir is at 0x0000 */
	movl %eax, %cr3			/* cr3 - page directory start */
	movl %cr0, %eax
	orl $0x80000000, %eax
	movl %eax, %cr0			/* set paging (PG) bit */
	ret				/* this also flushes prefetch-queue */

/*=============================================================================
 * ORIGINAL DESCRIPTORS (Preserved)
 *============================================================================*/

.align 2
.word 0
idt_descr:
	.word 256*8-1			# idt contains 256 entries
	.long idt
.align 2
.word 0
gdt_descr:
	.word 256*8-1			# so does gdt
	.long gdt

	.align 8
idt:	.fill 256, 8, 0			# idt is uninitialized

gdt:	.quad 0x0000000000000000	/* NULL descriptor */
	.quad 0x00c09a0000000fff	/* 16Mb code segment */
	.quad 0x00c0920000000fff	/* 16Mb data segment */
	.quad 0x0000000000000000	/* TEMPORARY - don't use */
	.fill 252, 8, 0			/* space for LDT's and TSS's etc */

/*=============================================================================
 * EARLY PRINTK FUNCTION (for use before servers)
 *============================================================================*/

.globl early_printk
early_printk:
	# Simplified version - just write to console via tty_write
	pushl %ebp
	movl %esp, %ebp
	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	
	movl 8(%ebp), %eax		# Get string pointer
	pushl %eax
	call strlen
	addl $4, %esp
	
	pushl %eax			# length
	pushl 8(%ebp)			# buffer
	pushl $0			# channel 0 (console)
	call tty_write
	addl $12, %esp
	
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	popl %ebp
	ret

/*=============================================================================
 * EXTERNAL REFERENCES
 *============================================================================*/

.extern main, tty_write, strlen
