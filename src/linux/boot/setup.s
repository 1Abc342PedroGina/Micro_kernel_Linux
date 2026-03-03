/*
* HISTORY
* $Log: setup.s,v $
* Revision 1.1 2026/02/15 15:15:30 pedro
* Microkernel version of setup code.
* Extended to initialize capability structures.
* Added server startup parameters.
* Preserved original BIOS parameter collection.
* [2026/02/15 pedro]
*/

/*
 * File: boot/setup.s
 * Author: Linus Torvalds (original Linux version)
 *         Falcon (AT&T syntax conversion)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * Setup code for microkernel architecture.
 *
 * Original Linux 0.11: Collects BIOS parameters and switches to protected mode.
 * Microkernel version: Additionally collects information needed for capability
 * system, sets up initial server descriptors, and prepares the environment
 * for multi-server boot.
 *
 * The BIOS parameters are extended to include information about:
 * - Available memory for capability domains
 * - Server locations in memory
 * - Initial capability tables
 */

	.code16
	.equ INITSEG, 0x9000	# we move boot here - out of the way
	.equ SYSSEG, 0x1000	# system loaded at 0x10000 (65536).
	.equ SETUPSEG, 0x9020	# this is the current segment

/*=============================================================================
 * MICROKERNEL CONSTANTS - Server segments (must match bootsect.s)
 *============================================================================*/

	.equ MEMSRV_SEG,  0x2000	# Memory server at 0x20000
	.equ PROCSRV_SEG, 0x3000	# Process server at 0x30000
	.equ FILESRV_SEG, 0x4000	# File server at 0x40000
	.equ DEVSRV_SEG,  0x5000	# Device server at 0x50000
	.equ TIMESRV_SEG, 0x6000	# Time server at 0x60000

	.equ NR_SERVERS,  5		# Number of servers

/*=============================================================================
 * CAPABILITY TABLE LOCATION
 *============================================================================*/

	.equ CAP_TABLE_SEG, 0x7000	# Capability table at 0x70000
	.equ KERNEL_STATE_SEG, 0x8000	# Kernel state at 0x80000

	.global _start, begtext, begdata, begbss, endtext, enddata, endbss
	.text
	begtext:
	.data
	begdata:
	.bss
	begbss:
	.text

	ljmp $SETUPSEG, $_start	
_start:
	mov %cs,%ax
	mov %ax,%ds
	mov %ax,%es

/*=============================================================================
 * PRINT SETUP MESSAGE (Extended)
 *============================================================================*/

	mov $0x03, %ah
	xor %bh, %bh
	int $0x10

	mov $45, %cx
	mov $0x000b, %bx
	mov $msg2, %bp
	mov $0x1301, %ax
	int $0x10

/*=============================================================================
 * ORIGINAL BIOS PARAMETER COLLECTION (Preserved)
 *============================================================================*/

# Save cursor position
	mov	$INITSEG, %ax
	mov	%ax, %ds
	mov	$0x03, %ah
	xor	%bh, %bh
	int	$0x10
	mov	%dx, %ds:0

# Get memory size (extended mem, kB)
	mov	$0x88, %ah 
	int	$0x15
	mov	%ax, %ds:2

# Get video-card data
	mov	$0x0f, %ah
	int	$0x10
	mov	%bx, %ds:4	# bh = display page
	mov	%ax, %ds:6	# al = video mode, ah = window width

# Check for EGA/VGA and some config parameters
	mov	$0x12, %ah
	mov	$0x10, %bl
	int	$0x10
	mov	%ax, %ds:8
	mov	%bx, %ds:10
	mov	%cx, %ds:12

# Get hd0 data
	mov	$0x0000, %ax
	mov	%ax, %ds
	lds	%ds:4*0x41, %si
	mov	$INITSEG, %ax
	mov	%ax, %es
	mov	$0x0080, %di
	mov	$0x10, %cx
	rep
	movsb

# Get hd1 data
	mov	$0x0000, %ax
	mov	%ax, %ds
	lds	%ds:4*0x46, %si
	mov	$INITSEG, %ax
	mov	%ax, %es
	mov	$0x0090, %di
	mov	$0x10, %cx
	rep
	movsb

/*=============================================================================
 * MICROKERNEL EXTENSION - Collect server information
 *============================================================================*/

	# Store server segments in BIOS parameter area (at 0x9000:0x00A0)
	mov	$INITSEG, %ax
	mov	%ax, %ds
	mov	$MEMSRV_SEG, %ax
	mov	%ax, %ds:0x00A0	# Memory server segment
	mov	$PROCSRV_SEG, %ax
	mov	%ax, %ds:0x00A2	# Process server segment
	mov	$FILESRV_SEG, %ax
	mov	%ax, %ds:0x00A4	# File server segment
	mov	$DEVSRV_SEG, %ax
	mov	%ax, %ds:0x00A6	# Device server segment
	mov	$TIMESRV_SEG, %ax
	mov	%ax, %ds:0x00A8	# Time server segment
	mov	$NR_SERVERS, %ax
	mov	%ax, %ds:0x00AA	# Number of servers

/*=============================================================================
 * ORIGINAL DISPLAY ROUTINES (Preserved)
 *============================================================================*/

## modify ds
	mov $INITSEG,%ax
	mov %ax,%ds
	mov $SETUPSEG,%ax
	mov %ax,%es

##show cursor pos:
	mov $0x03, %ah 
	xor %bh,%bh
	int $0x10
	mov $11,%cx
	mov $0x000c,%bx
	mov $cur,%bp
	mov $0x1301,%ax
	int $0x10
##show detail
	mov %ds:0 ,%ax
	call print_hex
	call print_nl

##show memory size
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10
	mov $12, %cx
	mov $0x000a, %bx
	mov $mem, %bp
	mov $0x1301, %ax
	int $0x10
##show detail
	mov %ds:2 , %ax
	call print_hex

##show cylinders
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10
	mov $25, %cx
	mov $0x000d, %bx
	mov $cyl, %bp
	mov $0x1301, %ax
	int $0x10
##show detail
	mov %ds:0x80, %ax
	call print_hex
	call print_nl

##show heads
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10
	mov $8, %cx
	mov $0x000e, %bx
	mov $head, %bp
	mov $0x1301, %ax
	int $0x10
##show detail
	mov %ds:0x82, %ax
	call print_hex
	call print_nl

##show sectors
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10
	mov $8, %cx
	mov $0x000f, %bx
	mov $sect, %bp
	mov $0x1301, %ax
	int $0x10
##show detail
	mov %ds:0x8e, %ax
	call print_hex
	call print_nl

/*=============================================================================
 * MICROKERNEL EXTENSION - Show server information
 *============================================================================*/

	# Show servers loaded
	mov $0x03, %ah
	xor %bh, %bh
	int $0x10
	mov $19, %cx
	mov $0x0010, %bx
	mov $srv_msg, %bp
	mov $0x1301, %ax
	int $0x10

	mov $NR_SERVERS, %ax
	call print_dec
	call print_nl

/*=============================================================================
 * ORIGINAL CHECK FOR SECOND DISK (Preserved)
 *============================================================================*/

# Check that there IS a hd1 :-)
	mov	$0x01500, %ax
	mov	$0x81, %dl
	int	$0x13
	jc	no_disk1
	cmp	$3, %ah
	je	is_disk1
no_disk1:
	mov	$INITSEG, %ax
	mov	%ax, %es
	mov	$0x0090, %di
	mov	$0x10, %cx
	mov	$0x00, %ax
	rep
	stosb
is_disk1:

/*=============================================================================
 * MICROKERNEL EXTENSION - Initialize capability table
 *============================================================================*/

	# Clear capability table
	mov	$CAP_TABLE_SEG, %ax
	mov	%ax, %es
	xor	%di, %di
	mov	$2048, %cx		# 2KB table (256 entries * 8 bytes)
	xor	%ax, %ax
	cld
	rep
	stosb

	# Initialize kernel capability (entry 0)
	mov	$CAP_TABLE_SEG, %ax
	mov	%ax, %es
	xor	%di, %di
	mov	$0xFFFFFFFF, %es:(%di)	# object_id = -1 (kernel)
	mov	$0x0000FFFF, %es:4(%di)	# rights = all

/*=============================================================================
 * ORIGINAL PROTECTED MODE SWITCH (Modified for microkernel)
 *============================================================================*/

	cli			# no interrupts allowed ! 

# Move system to its rightful place (original)
	mov	$0x0000, %ax
	cld			# 'direction'=0, movs moves forward
do_move:
	mov	%ax, %es	# destination segment
	add	$0x1000, %ax
	cmp	$0x9000, %ax
	jz	end_move
	mov	%ax, %ds	# source segment
	sub	%di, %di
	sub	%si, %si
	mov 	$0x8000, %cx
	rep
	movsw
	jmp	do_move

end_move:
	mov	$SETUPSEG, %ax
	mov	%ax, %ds
	lidt	idt_48
	lgdt	gdt_48

# Enable A20 line (original)
	inb     $0x92, %al
	orb     $0b00000010, %al
	outb    %al, $0x92

# Reprogram interrupts (original)
	mov	$0x11, %al
	out	%al, $0x20
	.word	0x00eb,0x00eb
	out	%al, $0xA0
	.word	0x00eb,0x00eb
	mov	$0x20, %al
	out	%al, $0x21
	.word	0x00eb,0x00eb
	mov	$0x28, %al
	out	%al, $0xA1
	.word	0x00eb,0x00eb
	mov	$0x04, %al
	out	%al, $0x21
	.word	0x00eb,0x00eb
	mov	$0x02, %al
	out	%al, $0xA1
	.word	0x00eb,0x00eb
	mov	$0x01, %al
	out	%al, $0x21
	.word	0x00eb,0x00eb
	out	%al, $0xA1
	.word	0x00eb,0x00eb
	mov	$0xFF, %al
	out	%al, $0x21
	.word	0x00eb,0x00eb
	out	%al, $0xA1

# Switch to protected mode
	mov	%cr0, %eax
	bts	$0, %eax
	mov	%eax, %cr0

# Jump to 32-bit code (kernel entry)
	.equ	sel_cs0, 0x0008
	ljmp	$sel_cs0, $0

/*=============================================================================
 * ORIGINAL UTILITY ROUTINES (Preserved)
 *============================================================================*/

empty_8042:
	.word	0x00eb,0x00eb
	in	$0x64, %al
	test	$2, %al
	jnz	empty_8042
	ret

/*=============================================================================
 * ORIGINAL PRINT ROUTINES (Preserved)
 *============================================================================*/

print_hex:
	mov $4,%cx
	mov %ax,%dx
print_digit:
	rol $4,%dx
	mov $0xe0f,%ax
	and %dl,%al
	add $0x30,%al
	cmp $0x3a,%al
	jl outp
	add $0x07,%al
outp:
	int $0x10
	loop print_digit
	ret

print_nl:
	mov $0xe0d,%ax
	int $0x10
	mov $0xa,%al
	int $0x10
	ret

print_dec:
	push %ax
	push %bx
	push %cx
	push %dx
	
	mov $0, %cx
	mov $10, %bx
	
div_loop:
	xor %dx, %dx
	div %bx
	push %dx
	inc %cx
	test %ax, %ax
	jnz div_loop
	
print_digits:
	pop %dx
	add $'0', %dl
	mov $0x0e, %ah
	int $0x10
	loop print_digits
	
	pop %dx
	pop %cx
	pop %bx
	pop %ax
	ret

/*=============================================================================
 * DATA AREAS
 *============================================================================*/

gdt:
	.word	0,0,0,0		# dummy

	.word	0x07FF		# 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		# base address=0
	.word	0x9A00		# code read/exec
	.word	0x00C0		# granularity=4096, 386

	.word	0x07FF		# 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		# base address=0
	.word	0x9200		# data read/write
	.word	0x00C0		# granularity=4096, 386

idt_48:
	.word	0			# idt limit=0
	.word	0,0			# idt base=0L

gdt_48:
	.word	0x800			# gdt limit=2048, 256 GDT entries
	.word   512+gdt, 0x9		# gdt base = 0X9xxxx

/*=============================================================================
 * MESSAGES (Extended)
 *============================================================================*/

msg2:
	.byte 13,10
	.ascii "IceCityOS Microkernel: Setup phase"
	.byte 13,10,13,10
cur:
	.ascii "Cursor POS:"
mem:
	.ascii "Memory SIZE:"
cyl:
	.ascii "KB"
	.byte 13,10,13,10
	.ascii "HD Info"
	.byte 13,10
	.ascii "Cylinders:"
head:
	.ascii "Headers:"
sect:
	.ascii "Sectors:"
srv_msg:
	.ascii "Servers loaded: "

.text
endtext:
.data
enddata:
.bss
endbss:
