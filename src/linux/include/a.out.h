/*
* HISTORY
* $Log: a.out.h,v $
* Revision 1.1 2026/02/14 14:30:45 pedro
* Microkernel version of a.out executable format.
* Original structures preserved for compatibility.
* Added capability-aware loading information.
* Extended for multiple executable formats.
* [2026/02/14 pedro]
*/

/*
* File: a.out.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* A.out executable format for microkernel architecture.
*
* Original Linux 0.11: Standard a.out executable format structures.
* Microkernel version: Same structures preserved for compatibility,
* but now used by the process server to load executables with
* capability-aware memory allocation.
*
* The process server reads the a.out header and uses it to:
* 1. Allocate capability-protected memory regions
* 2. Set up appropriate permissions (text = read/exec, data = read/write)
* 3. Create capability spaces for the new task
* 4. Grant initial capabilities based on executable's permissions
*/

#ifndef _A_OUT_H
#define _A_OUT_H

#define __GNU_EXEC_MACROS__

#include <sys/types.h>
#include <linux/kernel.h>
#include <linux/head.h>

/*=============================================================================
 * ORIGINAL A.OUT STRUCTURE (Preserved exactly)
 *============================================================================*/

struct exec {
  unsigned long a_magic;	/* Use macros N_MAGIC, etc for access */
  unsigned a_text;		/* length of text, in bytes */
  unsigned a_data;		/* length of data, in bytes */
  unsigned a_bss;		/* length of uninitialized data area for file, in bytes */
  unsigned a_syms;		/* length of symbol table data in file, in bytes */
  unsigned a_entry;		/* start address */
  unsigned a_trsize;		/* length of relocation info for text, in bytes */
  unsigned a_drsize;		/* length of relocation info for data, in bytes */
};

/*=============================================================================
 * ORIGINAL MAGIC NUMBERS AND MACROS (Preserved exactly)
 *============================================================================*/

#ifndef N_MAGIC
#define N_MAGIC(exec) ((exec).a_magic)
#endif

#ifndef OMAGIC
/* Code indicating object file or impure executable.  */
#define OMAGIC 0407
/* Code indicating pure executable.  */
#define NMAGIC 0410
/* Code indicating demand-paged executable.  */
#define ZMAGIC 0413
#endif /* not OMAGIC */

#ifndef N_BADMAG
#define N_BADMAG(x)					\
 (N_MAGIC(x) != OMAGIC && N_MAGIC(x) != NMAGIC		\
  && N_MAGIC(x) != ZMAGIC)
#endif

#define _N_BADMAG(x)					\
 (N_MAGIC(x) != OMAGIC && N_MAGIC(x) != NMAGIC		\
  && N_MAGIC(x) != ZMAGIC)

#define _N_HDROFF(x) (SEGMENT_SIZE - sizeof (struct exec))

#ifndef N_TXTOFF
#define N_TXTOFF(x) \
 (N_MAGIC(x) == ZMAGIC ? _N_HDROFF((x)) + sizeof (struct exec) : sizeof (struct exec))
#endif

#ifndef N_DATOFF
#define N_DATOFF(x) (N_TXTOFF(x) + (x).a_text)
#endif

#ifndef N_TRELOFF
#define N_TRELOFF(x) (N_DATOFF(x) + (x).a_data)
#endif

#ifndef N_DRELOFF
#define N_DRELOFF(x) (N_TRELOFF(x) + (x).a_trsize)
#endif

#ifndef N_SYMOFF
#define N_SYMOFF(x) (N_DRELOFF(x) + (x).a_drsize)
#endif

#ifndef N_STROFF
#define N_STROFF(x) (N_SYMOFF(x) + (x).a_syms)
#endif

/* Address of text segment in memory after it is loaded.  */
#ifndef N_TXTADDR
#define N_TXTADDR(x) 0
#endif

/* Address of data segment in memory after it is loaded.
   Note that it is up to you to define SEGMENT_SIZE
   on machines not listed here.  */
#if defined(vax) || defined(hp300) || defined(pyr)
#define SEGMENT_SIZE PAGE_SIZE
#endif
#ifdef	hp300
#define	PAGE_SIZE	4096
#endif
#ifdef	sony
#define	SEGMENT_SIZE	0x2000
#endif	/* Sony.  */
#ifdef is68k
#define SEGMENT_SIZE 0x20000
#endif
#if defined(m68k) && defined(PORTAR)
#define PAGE_SIZE 0x400
#define SEGMENT_SIZE PAGE_SIZE
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifndef SEGMENT_SIZE
#define SEGMENT_SIZE 1024
#endif

#define _N_SEGMENT_ROUND(x) (((x) + SEGMENT_SIZE - 1) & ~(SEGMENT_SIZE - 1))

#define _N_TXTENDADDR(x) (N_TXTADDR(x)+(x).a_text)

#ifndef N_DATADDR
#define N_DATADDR(x) \
    (N_MAGIC(x)==OMAGIC? (_N_TXTENDADDR(x)) \
     : (_N_SEGMENT_ROUND (_N_TXTENDADDR(x))))
#endif

/* Address of bss segment in memory after it is loaded.  */
#ifndef N_BSSADDR
#define N_BSSADDR(x) (N_DATADDR(x) + (x).a_data)
#endif

/*=============================================================================
 * ORIGINAL SYMBOL TABLE STRUCTURE (Preserved)
 *============================================================================*/

#ifndef N_NLIST_DECLARED
struct nlist {
  union {
    char *n_name;
    struct nlist *n_next;
    long n_strx;
  } n_un;
  unsigned char n_type;
  char n_other;
  short n_desc;
  unsigned long n_value;
};
#endif

/*=============================================================================
 * ORIGINAL SYMBOL TYPE CONSTANTS (Preserved)
 *============================================================================*/

#ifndef N_UNDF
#define N_UNDF 0
#endif
#ifndef N_ABS
#define N_ABS 2
#endif
#ifndef N_TEXT
#define N_TEXT 4
#endif
#ifndef N_DATA
#define N_DATA 6
#endif
#ifndef N_BSS
#define N_BSS 8
#endif
#ifndef N_COMM
#define N_COMM 18
#endif
#ifndef N_FN
#define N_FN 15
#endif

#ifndef N_EXT
#define N_EXT 1
#endif
#ifndef N_TYPE
#define N_TYPE 036
#endif
#ifndef N_STAB
#define N_STAB 0340
#endif

/* The following type indicates the definition of a symbol as being
   an indirect reference to another symbol.  The other symbol
   appears as an undefined reference, immediately following this symbol.

   Indirection is asymmetrical.  The other symbol's value will be used
   to satisfy requests for the indirect symbol, but not vice versa.
   If the other symbol does not have a definition, libraries will
   be searched to find a definition.  */
#define N_INDR 0xa

/* The following symbols refer to set elements.
   All the N_SET[ATDB] symbols with the same name form one set.
   Space is allocated for the set in the text section, and each set
   element's value is stored into one word of the space.
   The first word of the space is the length of the set (number of elements).

   The address of the set is made into an N_SETV symbol
   whose name is the same as the name of the set.
   This symbol acts like a N_DATA global symbol
   in that it can satisfy undefined external references.  */

/* These appear as input to LD, in a .o file.  */
#define	N_SETA	0x14		/* Absolute set element symbol */
#define	N_SETT	0x16		/* Text set element symbol */
#define	N_SETD	0x18		/* Data set element symbol */
#define	N_SETB	0x1A		/* Bss set element symbol */

/* This is output from LD.  */
#define N_SETV	0x1C		/* Pointer to set vector in data area.  */

/*=============================================================================
 * ORIGINAL RELOCATION STRUCTURE (Preserved)
 *============================================================================*/

#ifndef N_RELOCATION_INFO_DECLARED

/* This structure describes a single relocation to be performed.
   The text-relocation section of the file is a vector of these structures,
   all of which apply to the text section.
   Likewise, the data-relocation section applies to the data section.  */

struct relocation_info
{
  /* Address (within segment) to be relocated.  */
  int r_address;
  /* The meaning of r_symbolnum depends on r_extern.  */
  unsigned int r_symbolnum:24;
  /* Nonzero means value is a pc-relative offset
     and it should be relocated for changes in its own address
     as well as for changes in the symbol or section specified.  */
  unsigned int r_pcrel:1;
  /* Length (as exponent of 2) of the field to be relocated.
     Thus, a value of 2 indicates 1<<2 bytes.  */
  unsigned int r_length:2;
  /* 1 => relocate with value of symbol.
          r_symbolnum is the index of the symbol
	  in file's the symbol table.
     0 => relocate with the address of a segment.
          r_symbolnum is N_TEXT, N_DATA, N_BSS or N_ABS
	  (the N_EXT bit may be set also, but signifies nothing).  */
  unsigned int r_extern:1;
  /* Four bits that aren't used, but when writing an object file
     it is desirable to clear them.  */
  unsigned int r_pad:4;
};
#endif /* no N_RELOCATION_INFO_DECLARED.  */

/*=============================================================================
 * MICROKERNEL EXTENSIONS FOR CAPABILITY-AWARE EXECUTABLE LOADING
 *============================================================================*/

/*=============================================================================
 * IPC Message Codes for Executable Loading
 *============================================================================*/

#define MSG_EXEC_LOAD		0x1100	/* Load executable */
#define MSG_EXEC_LOAD_SEGMENT	0x1101	/* Load a segment */
#define MSG_EXEC_RELOCATE	0x1102	/* Perform relocation */
#define MSG_EXEC_SYMBOL		0x1103	/* Look up symbol */
#define MSG_EXEC_REPLY		0x1104	/* Reply from process server */

/*=============================================================================
 * Executable Loading Message Structures
 *============================================================================*/

struct msg_exec_load {
	struct mk_msg_header header;
	const char *filename;		/* Executable file name */
	struct exec *header_out;	/* Output: a.out header */
	unsigned int task_id;		/* Task making request */
	capability_t caps;		/* Caller capabilities */
	unsigned int flags;		/* Load flags */
};

struct msg_exec_load_segment {
	struct mk_msg_header header;
	unsigned int task_id;		/* Target task ID */
	int segment_type;		/* N_TEXT, N_DATA, N_BSS */
	unsigned long vaddr;		/* Virtual address */
	unsigned long size;		/* Segment size */
	unsigned long file_offset;	/* Offset in file */
	unsigned int prot;		/* Memory protection (MEM_PROT_*) */
	capability_t caps;		/* Caller capabilities */
};

struct msg_exec_relocate {
	struct mk_msg_header header;
	unsigned int task_id;		/* Target task ID */
	struct relocation_info *rel;	/* Relocation info */
	int count;			/* Number of relocations */
	unsigned long text_addr;	/* Text segment address */
	unsigned long data_addr;	/* Data segment address */
	capability_t caps;		/* Caller capabilities */
};

struct msg_exec_reply {
	struct mk_msg_header header;
	int result;			/* Result code */
	union {
		struct exec header;	/* Executable header */
		unsigned long addr;	/* Loaded address */
		unsigned long value;	/* Symbol value */
	} data;
	capability_t granted_caps;	/* New capabilities granted */
};

/*=============================================================================
 * Capability Flags for Executable Loading
 *============================================================================*/

#define CAP_EXEC_LOAD		0x4000	/* Can load executables */
#define CAP_EXEC_SEGMENT	0x8000	/* Can map executable segments */

/*=============================================================================
 * Executable Loading Flags
 *============================================================================*/

#define EXEC_LOAD_NORMAL	0x00	/* Normal loading */
#define EXEC_LOAD_DEBUG		0x01	/* Debug mode */
#define EXEC_LOAD_NOW		0x02	/* Load immediately (no demand paging) */
#define EXEC_LOAD_CAP_CHECK	0x04	/* Check capabilities strictly */

/*=============================================================================
 * Segment Types (extending N_* constants)
 *============================================================================*/

#define SEGMENT_TYPE_MASK	0xFF
#define SEGMENT_TEXT		N_TEXT	/* Text segment */
#define SEGMENT_DATA		N_DATA	/* Data segment */
#define SEGMENT_BSS		N_BSS	/* BSS segment */
#define SEGMENT_STACK		0x10	/* Stack segment (new) */
#define SEGMENT_HEAP		0x11	/* Heap segment (new) */
#define SEGMENT_TLS		0x12	/* Thread-local storage (new) */
#define SEGMENT_GUARD		0x13	/* Guard page (new) */

/*=============================================================================
 * Memory Protection Flags for Segments
 *============================================================================*/

#define SEG_PROT_READ		MEM_PROT_READ
#define SEG_PROT_WRITE		MEM_PROT_WRITE
#define SEG_PROT_EXEC		MEM_PROT_EXEC
#define SEG_PROT_COW		MEM_PROT_COW
#define SEG_PROT_SHARED		MEM_PROT_SHARED

/* Default protections per segment type */
#define SEG_PROT_TEXT		(SEG_PROT_READ | SEG_PROT_EXEC)
#define SEG_PROT_DATA		(SEG_PROT_READ | SEG_PROT_WRITE)
#define SEG_PROT_BSS		(SEG_PROT_READ | SEG_PROT_WRITE)
#define SEG_PROT_STACK		(SEG_PROT_READ | SEG_PROT_WRITE)
#define SEG_PROT_HEAP		(SEG_PROT_READ | SEG_PROT_WRITE)

/*=============================================================================
 * Executable Format Detection
 *============================================================================*/

/* Known executable formats */
#define EXEC_FORMAT_UNKNOWN	0
#define EXEC_FORMAT_AOUT	1	/* a.out */
#define EXEC_FORMAT_ELF		2	/* ELF */
#define EXEC_FORMAT_COFF	3	/* COFF */
#define EXEC_FORMAT_MICRO	4	/* Microkernel-specific format */

/**
 * detect_exec_format - Detect executable format from magic number
 * @magic: First 4 bytes of file
 * 
 * Returns EXEC_FORMAT_* constant.
 */
static inline int detect_exec_format(unsigned long magic)
{
	if (magic == OMAGIC || magic == NMAGIC || magic == ZMAGIC)
		return EXEC_FORMAT_AOUT;
	
	/* ELF magic: 0x7F 'E' 'L' 'F' */
	if ((magic & 0xFFFFFF) == 0x464C457F)
		return EXEC_FORMAT_ELF;
	
	return EXEC_FORMAT_UNKNOWN;
}

/*=============================================================================
 * Capability Space Layout for Executables
 *============================================================================*/

/**
 * Default capability space layout for new tasks:
 * - Text segment: Read/Execute capability
 * - Data segment: Read/Write capability  
 * - Stack: Read/Write capability (grows down)
 * - Heap: Read/Write capability (grows up)
 */
#define DEFAULT_TEXT_CAPS	(CAP_RIGHT_READ | CAP_RIGHT_EXEC)
#define DEFAULT_DATA_CAPS	(CAP_RIGHT_READ | CAP_RIGHT_WRITE)
#define DEFAULT_STACK_CAPS	(CAP_RIGHT_READ | CAP_RIGHT_WRITE)
#define DEFAULT_HEAP_CAPS	(CAP_RIGHT_READ | CAP_RIGHT_WRITE)

/* Default stack size (in pages) */
#define DEFAULT_STACK_PAGES	16	/* 64KB with 4K pages */
#define DEFAULT_STACK_SIZE	(DEFAULT_STACK_PAGES * PAGE_SIZE)

/* Default heap initial size */
#define DEFAULT_HEAP_PAGES	4	/* 16KB initial heap */
#define DEFAULT_HEAP_SIZE	(DEFAULT_HEAP_PAGES * PAGE_SIZE)

/*=============================================================================
 * Capability-Aware Address Calculation
 *============================================================================*/

/**
 * N_TXTADDR_CAP - Text address with capability space offset
 * @x: Executable header
 * @space_id: Capability space ID
 * 
 * Returns text address within the specified capability space.
 */
#define N_TXTADDR_CAP(x, space_id) \
	(N_TXTADDR(x) + (space_id * CAPABILITY_SPACE_SIZE))

/**
 * N_DATADDR_CAP - Data address with capability space offset
 * @x: Executable header
 * @space_id: Capability space ID
 */
#define N_DATADDR_CAP(x, space_id) \
	(N_DATADDR(x) + (space_id * CAPABILITY_SPACE_SIZE))

/**
 * N_BSSADDR_CAP - BSS address with capability space offset
 * @x: Executable header
 * @space_id: Capability space ID
 */
#define N_BSSADDR_CAP(x, space_id) \
	(N_BSSADDR(x) + (space_id * CAPABILITY_SPACE_SIZE))

/*=============================================================================
 * Helper Functions for Executable Loading
 *============================================================================*/

/**
 * validate_exec_header - Validate a.out header
 * @exec: Executable header
 * 
 * Returns 1 if header is valid, 0 otherwise.
 */
static inline int validate_exec_header(struct exec *exec)
{
	if (!exec)
		return 0;
	
	/* Check magic number */
	if (N_BADMAG(*exec))
		return 0;
	
	/* Basic sanity checks */
	if (exec->a_text > 1024*1024 ||	/* Text > 1MB? */
	    exec->a_data > 1024*1024 ||	/* Data > 1MB? */
	    exec->a_bss > 1024*1024)	/* BSS > 1MB? */
		return 0;
	
	return 1;
}

/**
 * get_segment_protection - Get default protection for segment type
 * @segment_type: N_TEXT, N_DATA, N_BSS, etc.
 * 
 * Returns protection flags (SEG_PROT_*).
 */
static inline unsigned int get_segment_protection(int segment_type)
{
	switch (segment_type) {
		case N_TEXT:
			return SEG_PROT_TEXT;
		case N_DATA:
			return SEG_PROT_DATA;
		case N_BSS:
			return SEG_PROT_BSS;
		default:
			return SEG_PROT_READ | SEG_PROT_WRITE;
	}
}

/**
 * get_segment_name - Get name of segment type
 * @segment_type: Segment type
 * 
 * Returns string name for debugging.
 */
static inline const char *get_segment_name(int segment_type)
{
	switch (segment_type) {
		case N_TEXT: return "text";
		case N_DATA: return "data";
		case N_BSS: return "bss";
		case SEGMENT_STACK: return "stack";
		case SEGMENT_HEAP: return "heap";
		default: return "unknown";
	}
}

/*=============================================================================
 * Process Server Interface Functions
 *============================================================================*/

/**
 * exec_load - Load executable for a new task
 * @filename: Executable file name
 * @task_id: Task ID to load for (0 for new task)
 * @header_out: Output buffer for a.out header
 * 
 * Sends request to process server to load executable.
 * Returns 0 on success, -1 on error.
 */
static inline int exec_load(const char *filename, unsigned int task_id, 
                             struct exec *header_out)
{
	struct msg_exec_load msg;
	struct msg_exec_reply reply;
	unsigned int reply_size = sizeof(reply);
	int result;

	if (!(current_capability & CAP_EXEC_LOAD))
		return -1;

	msg.header.msg_id = MSG_EXEC_LOAD;
	msg.header.sender_port = kernel_state->kernel_port;
	msg.header.reply_port = kernel_state->kernel_port;
	msg.header.size = sizeof(msg);

	msg.filename = filename;
	msg.header_out = header_out;
	msg.task_id = task_id;
	msg.caps = current_capability;
	msg.flags = EXEC_LOAD_NORMAL;

	result = mk_msg_send(kernel_state->process_server, &msg, sizeof(msg));
	if (result < 0)
		return -1;

	result = mk_msg_receive(kernel_state->kernel_port, &reply, &reply_size);
	if (result < 0)
		return -1;

	if (reply.result == 0 && header_out)
		*header_out = reply.data.header;

	return reply.result;
}

#endif /* _A_OUT_H */
