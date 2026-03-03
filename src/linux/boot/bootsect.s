/*
* HISTORY
* $Log: bootsect.s,v $
* Revision 1.1 2026/02/15 14:15:30 pedro
* Microkernel version of boot sector.
* Extended to load multiple server binaries.
* Added server loading after kernel.
* Preserved original boot logic.
* [2026/02/15 pedro]
*/

/*
 * File: boot/bootsect.s
 * Author: Linus Torvalds (original Linux version)
 *         Falcon (AT&T syntax conversion)
 *         Pedro Emanuel (microkernel adaptation)
 * Date: 2026/02/15
 *
 * Boot sector for microkernel architecture.
 *
 * Original Linux 0.11: Loads setup and kernel (system).
 * Microkernel version: Loads setup, kernel, and additional server binaries
 * (memory server, process server, file server, etc.) into memory.
 *
 * The boot process now:
 * 1. Moves itself to 0x90000
 * 2. Loads setup sectors (as before)
 * 3. Loads kernel at 0x10000
 * 4. Loads server binaries at consecutive addresses
 * 5. Jumps to setup code
 */

	.code16
	.global _start, begtext, begdata, begbss, endtext, enddata, endbss
	.text
	begtext:
	.data
	begdata:
	.bss
	begbss:
	.text

/*=============================================================================
 * ORIGINAL CONSTANTS (Preserved)
 *============================================================================*/

	.equ SYSSIZE, 0x3000		# kernel size (clicks - 16 bytes)
	.equ SETUPLEN, 4		# nr of setup-sectors
	.equ BOOTSEG, 0x07c0		# original address of boot-sector
	.equ INITSEG, 0x9000		# we move boot here - out of the way
	.equ SETUPSEG, 0x9020		# setup starts here
	.equ SYSSEG, 0x1000		# system loaded at 0x10000 (65536).
	.equ ENDSEG, SYSSEG + SYSSIZE	# where to stop loading kernel

/*=============================================================================
 * MICROKERNEL CONSTANTS - Server locations
 *============================================================================*/

	.equ NR_SERVERS, 5		# Number of servers to load
	
	# Server segments (each gets 64KB = 0x1000 paragraphs)
	.equ MEMSRV_SEG,  0x2000	# Memory server at 0x20000
	.equ PROCSRV_SEG, 0x3000	# Process server at 0x30000
	.equ FILESRV_SEG, 0x4000	# File server at 0x40000
	.equ DEVSRV_SEG,  0x5000	# Device server at 0x50000
	.equ TIMESRV_SEG, 0x6000	# Time server at 0x60000
	
	# Server sizes (in clicks, to be filled by build process)
	.equ MEMSRV_SIZE, 0		# Will be set by linker
	.equ PROCSRV_SIZE, 0		# Will be set by linker
	.equ FILESRV_SIZE, 0		# Will be set by linker
	.equ DEVSRV_SIZE, 0		# Will be set by linker
	.equ TIMESRV_SIZE, 0		# Will be set by linker

/*=============================================================================
 * ORIGINAL ROOT_DEV
 *============================================================================*/

	.equ ROOT_DEV, 0x301		# first partition on first drive

/*=============================================================================
 * ORIGINAL STARTUP CODE (Modified)
 *============================================================================*/

	ljmp    $BOOTSEG, $_start
_start:
	mov	$BOOTSEG, %ax		# set ds to 0x7C0
	mov	%ax, %ds
	mov	$INITSEG, %ax		# set es to 0x9000
	mov	%ax, %es
	mov	$256, %cx		# move 256 words
	sub	%si, %si		# source ds:si = 0x07C0:0x0000
	sub	%di, %di		# destination es:di = 0x9000:0x0000
	rep
	movsw				# move itself to 0x90000
	ljmp	$INITSEG, $go		# jump to new location

go:	mov	%cs, %ax		# set segments to new location
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %ss
	mov	$0xFF00, %sp		# set stack

/*=============================================================================
 * LOAD SETUP SECTORS (Original)
 *============================================================================*/

load_setup:
	mov	$0x0000, %dx		# drive 0, head 0
	mov	$0x0002, %cx		# sector 2, track 0
	mov	$0x0200, %bx		# address = 512, in INITSEG
	.equ    AX, 0x0200+SETUPLEN
	mov     $AX, %ax		# service 2, nr of sectors
	int	$0x13			# read it
	jnc	ok_load_setup		# ok - continue
	mov	$0x0000, %dx
	mov	$0x0000, %ax		# reset the diskette
	int	$0x13
	jmp	load_setup

ok_load_setup:

/*=============================================================================
 * GET DISK PARAMETERS (Original)
 *============================================================================*/

	mov	$0x00, %dl
	mov	$0x0800, %ax		# AH=8 is get drive parameters
	int	$0x13
	mov	$0x00, %ch
	mov	%cx, %cs:sectors+0	# save sectors per track
	mov	$INITSEG, %ax
	mov	%ax, %es

/*=============================================================================
 * PRINT BOOT MESSAGE (Modified)
 *============================================================================*/

	mov	$0x03, %ah		# read cursor pos
	xor	%bh, %bh
	int	$0x10
	
	mov	$39, %cx		# length of message
	mov	$0x0007, %bx		# page 0, attribute 7
	mov     $msg1, %bp
	mov	$0x1301, %ax		# write string, move cursor
	int	$0x10

/*=============================================================================
 * LOAD KERNEL (Original)
 *============================================================================*/

	mov	$SYSSEG, %ax
	mov	%ax, %es		# segment of 0x010000
	call	read_it			# read kernel

/*=============================================================================
 * MICROKERNEL EXTENSION - LOAD SERVERS
 *============================================================================*/

	# Load memory server
	mov	$MEMSRV_SEG, %ax
	mov	%ax, %es
	mov	$MEMSRV_SIZE, %cx	# size in clicks
	call	read_servers

	# Load process server
	mov	$PROCSRV_SEG, %ax
	mov	%ax, %es
	mov	$PROCSRV_SIZE, %cx
	call	read_servers

	# Load file server
	mov	$FILESRV_SEG, %ax
	mov	%ax, %es
	mov	$FILESRV_SIZE, %cx
	call	read_servers

	# Load device server
	mov	$DEVSRV_SEG, %ax
	mov	%ax, %es
	mov	$DEVSRV_SIZE, %cx
	call	read_servers

	# Load time server
	mov	$TIMESRV_SEG, %ax
	mov	%ax, %es
	mov	$TIMESRV_SIZE, %cx
	call	read_servers

/*=============================================================================
 * TURN OFF MOTOR (Original)
 *============================================================================*/

	call	kill_motor

/*=============================================================================
 * DETERMINE ROOT DEVICE (Original)
 *============================================================================*/

	#seg cs
	mov	%cs:root_dev+0, %ax
	cmp	$0, %ax
	jne	root_defined
	#seg cs
	mov	%cs:sectors+0, %bx
	mov	$0x0208, %ax		# /dev/ps0 - 1.2Mb
	cmp	$15, %bx
	je	root_defined
	mov	$0x021c, %ax		# /dev/PS0 - 1.44Mb
	cmp	$18, %bx
	je	root_defined
undef_root:
	jmp undef_root
root_defined:
	#seg cs
	mov	%ax, %cs:root_dev+0

/*=============================================================================
 * JUMP TO SETUP (Original)
 *============================================================================*/

	ljmp	$SETUPSEG, $0

/*=============================================================================
 * ORIGINAL READ_IT ROUTINE (Preserved)
 *============================================================================*/

sread:	.word 1+ SETUPLEN	# sectors read of current track
head:	.word 0			# current head
track:	.word 0			# current track

read_it:
	mov	%es, %ax
	test	$0x0fff, %ax
die:	jne 	die			# es must be at 64kB boundary
	xor 	%bx, %bx		# bx is starting address within segment
rp_read:
	mov 	%es, %ax
 	cmp 	$ENDSEG, %ax		# have we loaded all yet?
	jb	ok1_read
	ret
ok1_read:
	#seg cs
	mov	%cs:sectors+0, %ax
	sub	sread, %ax
	mov	%ax, %cx
	shl	$9, %cx
	add	%bx, %cx
	jnc 	ok2_read
	je 	ok2_read
	xor 	%ax, %ax
	sub 	%bx, %ax
	shr 	$9, %ax
ok2_read:
	call 	read_track
	mov 	%ax, %cx
	add 	sread, %ax
	#seg cs
	cmp 	%cs:sectors+0, %ax
	jne 	ok3_read
	mov 	$1, %ax
	sub 	head, %ax
	jne 	ok4_read
	incw    track 
ok4_read:
	mov	%ax, head
	xor	%ax, %ax
ok3_read:
	mov	%ax, sread
	shl	$9, %cx
	add	%cx, %bx
	jnc	rp_read
	mov	%es, %ax
	add	$0x1000, %ax
	mov	%ax, %es
	xor	%bx, %bx
	jmp	rp_read

/*=============================================================================
 * MICROKERNEL READ_SERVERS ROUTINE
 *============================================================================*/

/*
 * read_servers - Load server binary
 * Input:
 *   es - segment to load to
 *   cx - size in clicks (16-byte paragraphs)
 */
read_servers:
	push	%ax
	push	%bx
	push	%cx
	push	%dx
	
	# Check if size is zero (server not included)
	cmp	$0, %cx
	je	rs_done
	
	# Save end segment
	mov	%es, %ax
	add	%cx, %ax
	mov	%ax, %cs:server_end+0
	
	xor	%bx, %bx		# start at offset 0
	
rs_rp_read:
	mov	%es, %ax
	cmp	%cs:server_end+0, %ax	# loaded all?
	jb	rs_ok1
	jmp	rs_done
	
rs_ok1:
	#seg cs
	mov	%cs:sectors+0, %ax
	sub	sread, %ax
	mov	%ax, %cx
	shl	$9, %cx
	add	%bx, %cx
	jnc 	rs_ok2
	je 	rs_ok2
	xor 	%ax, %ax
	sub 	%bx, %ax
	shr 	$9, %ax
	
rs_ok2:
	call 	read_track
	mov 	%ax, %cx
	add 	sread, %ax
	#seg cs
	cmp 	%cs:sectors+0, %ax
	jne 	rs_ok3
	mov 	$1, %ax
	sub 	head, %ax
	jne 	rs_ok4
	incw    track 
rs_ok4:
	mov	%ax, head
	xor	%ax, %ax
	
rs_ok3:
	mov	%ax, sread
	shl	$9, %cx
	add	%cx, %bx
	jnc	rs_rp_read
	mov	%es, %ax
	add	$0x1000, %ax
	mov	%ax, %es
	xor	%bx, %bx
	jmp	rs_rp_read

rs_done:
	pop	%dx
	pop	%cx
	pop	%bx
	pop	%ax
	ret

server_end:
	.word 0

/*=============================================================================
 * ORIGINAL READ_TRACK ROUTINE (Preserved)
 *============================================================================*/

read_track:
	push	%ax
	push	%bx
	push	%cx
	push	%dx
	mov	track, %dx
	mov	sread, %cx
	inc	%cx
	mov	%dl, %ch
	mov	head, %dx
	mov	%dl, %dh
	mov	$0, %dl
	and	$0x0100, %dx
	mov	$2, %ah
	int	$0x13
	jc	bad_rt
	pop	%dx
	pop	%cx
	pop	%bx
	pop	%ax
	ret
bad_rt:	mov	$0, %ax
	mov	$0, %dx
	int	$0x13
	pop	%dx
	pop	%cx
	pop	%bx
	pop	%ax
	jmp	read_track

/*=============================================================================
 * ORIGINAL KILL_MOTOR (Preserved)
 *============================================================================*/

kill_motor:
	push	%dx
	mov	$0x3f2, %dx
	mov	$0, %al
	outsb
	pop	%dx
	ret

/*=============================================================================
 * DATA AREAS
 *============================================================================*/

sectors:
	.word 0

msg1:
	.byte 13,10
	.ascii "IceCityOS Microkernel loading kernel and servers..."
	.byte 13,10,13,10

	.org 508
root_dev:
	.word ROOT_DEV
boot_flag:
	.word 0xAA55
	
	.text
	endtext:
	.data
	enddata:
	.bss
	endbss:
