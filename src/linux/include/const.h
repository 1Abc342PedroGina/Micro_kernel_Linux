/*
* HISTORY
* $Log: const.h,v $
* Revision 1.1 2026/02/14 13:30:45 pedro
* Microkernel version of system constants.
* Original constants preserved for compatibility.
* Added capability and IPC-related constants.
* Memory layout constants now refer to capability spaces.
* [2026/02/14 pedro]
*/

/*
* File: const.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* System constants for microkernel architecture.
*
* Original Linux 0.11: Constants for buffer management and inode types.
* Microkernel version: Same constants preserved for compatibility, but now
* interpreted in the context of capability-based security:
* - BUFFER_END: End of buffer cache (now managed by file server)
* - I_* constants: Inode types (now represent memory object types)
*
* Note: This header is pure compile-time constants - no IPC needed.
*/

#ifndef _CONST_H
#define _CONST_H

/*=============================================================================
 * ORIGINAL CONSTANTS (Preserved exactly for compatibility)
 *============================================================================*/

/*
 * BUFFER_END - End of buffer cache memory
 * 
 * Original Linux 0.11: Physical memory address where buffer cache ends.
 * Microkernel version: This constant is now a hint to the file server
 * about how much buffer cache to allocate. The actual memory is managed
 * by the memory server based on capabilities.
 */
#define BUFFER_END 0x200000

/*
 * Inode type masks - used in file system operations
 * Original: Minix/FS inode types
 * Microkernel: Now represent memory object capability types
 */
#define I_TYPE          0170000	/* Type of inode mask */
#define I_DIRECTORY	0040000		/* Directory inode */
#define I_REGULAR       0100000		/* Regular file inode */
#define I_BLOCK_SPECIAL 0060000		/* Block special device inode */
#define I_CHAR_SPECIAL  0020000		/* Character special device inode */
#define I_NAMED_PIPE	0010000		/* Named pipe (FIFO) inode */

/* Set-UID/Set-GID bits (preserved for permission compatibility) */
#define I_SET_UID_BIT   0004000		/* Set UID on execution */
#define I_SET_GID_BIT   0002000		/* Set GID on execution */

/*=============================================================================
 * MICROKERNEL CONSTANTS (Extensions)
 *============================================================================*/

/*=============================================================================
 * Memory Layout Constants
 *============================================================================*/

/*
 * Memory layout for microkernel:
 * - CAPABILITY_SPACE_SIZE: Size of each capability domain's address space
 * - KERNEL_SPACE_START: Start of kernel capability space
 * - USER_SPACE_START: Start of user capability space
 * - SERVER_SPACE_START: Start of server capability space
 */
#define CAPABILITY_SPACE_SIZE	0x40000000	/* 1GB per capability domain */
#define KERNEL_SPACE_START	0x00000000	/* Kernel domain */
#define USER_SPACE_START	0x40000000	/* User domain 0 */
#define SERVER_SPACE_START	0x80000000	/* Server domain */

/* Number of capability domains supported */
#define MAX_CAPABILITY_DOMAINS	16

/*=============================================================================
 * IPC Constants
 *============================================================================*/

/* Maximum message sizes */
#define IPC_MAX_MSG_SIZE	4096	/* Maximum IPC message size */
#define IPC_MAX_DATA_SIZE	2032	/* Maximum data in message */
#define IPC_MAX_ARGS		9	/* Maximum arguments per call */

/* IPC timeouts (in milliseconds) */
#define IPC_TIMEOUT_NONE	0	/* No timeout (wait forever) */
#define IPC_TIMEOUT_DEFAULT	5000	/* Default timeout (5 seconds) */
#define IPC_TIMEOUT_MAX		30000	/* Maximum timeout (30 seconds) */

/* IPC queue limits */
#define IPC_MAX_PORTS		256	/* Maximum ports per task */
#define IPC_MAX_QUEUE		64	/* Maximum messages per port */
#define IPC_MAX_REPLY_QUEUE	16	/* Maximum pending replies */

/*=============================================================================
 * Capability Constants
 *============================================================================*/

/* Capability types */
#define CAP_TYPE_NONE		0	/* No capability */
#define CAP_TYPE_TASK		1	/* Task capability */
#define CAP_TYPE_PORT		2	/* Port capability */
#define CAP_TYPE_MEMORY		3	/* Memory capability */
#define CAP_TYPE_FILE		4	/* File capability */
#define CAP_TYPE_DEVICE		5	/* Device capability */
#define CAP_TYPE_SIGNAL		6	/* Signal capability */
#define CAP_TYPE_TIMER		7	/* Timer capability */

/* Capability rights (extended from original) */
#define CAP_RIGHT_READ		0x0001	/* Read capability */
#define CAP_RIGHT_WRITE		0x0002	/* Write capability */
#define CAP_RIGHT_EXEC		0x0004	/* Execute capability */
#define CAP_RIGHT_CREATE		0x0008	/* Create capability */
#define CAP_RIGHT_DELETE		0x0010	/* Delete capability */
#define CAP_RIGHT_CONTROL		0x0020	/* Control capability */
#define CAP_RIGHT_INHERIT		0x0040	/* Can be inherited */
#define CAP_RIGHT_COPY		0x0080	/* Can be copied */
#define CAP_RIGHT_SHARE		0x0100	/* Can be shared */
#define CAP_RIGHT_REVOKE		0x0200	/* Can be revoked */
#define CAP_RIGHT_GRANT		0x0400	/* Can grant to others */

/* Capability space IDs (for segment registers) */
#define CAP_SPACE_KERNEL	0	/* Kernel capability space */
#define CAP_SPACE_USER		1	/* User capability space */
#define CAP_SPACE_SERVER	2	/* Server capability space */
#define CAP_SPACE_IPC		3	/* IPC temporary space */
#define CAP_SPACE_MAX		15	/* Maximum space ID */

/*=============================================================================
 * Server Port Constants
 *============================================================================*/

/* Well-known server ports (must match head.h) */
#define PORT_BOOTSTRAP		0x0001	/* Bootstrap server */
#define PORT_KERNEL		0x0002	/* Kernel port */
#define PORT_MEMORY		0x0003	/* Memory server */
#define PORT_PROCESS		0x0004	/* Process server */
#define PORT_FILE		0x0005	/* File server */
#define PORT_DEVICE		0x0006	/* Device server */
#define PORT_SIGNAL		0x0007	/* Signal server */
#define PORT_TERMINAL		0x0008	/* Terminal server */
#define PORT_TIME		0x0009	/* Time server */
#define PORT_SYSTEM		0x000A	/* System server */
#define PORT_LOCALE		0x000B	/* Locale server */
#define PORT_NETWORK		0x000C	/* Network server */

/* Port ranges */
#define PORT_RESERVED_START	0x0001	/* Start of reserved ports */
#define PORT_RESERVED_END	0x00FF	/* End of reserved ports */
#define PORT_DYNAMIC_START	0x0100	/* Start of dynamic ports */
#define PORT_DYNAMIC_END	0xFFFF	/* End of dynamic ports */

/*=============================================================================
 * Task/Process Constants
 *============================================================================*/

/* Task states */
#define TASK_RUNNING		0	/* Task is running */
#define TASK_INTERRUPTIBLE	1	/* Task is interruptible sleeping */
#define TASK_UNINTERRUPTIBLE	2	/* Task is uninterruptible sleeping */
#define TASK_ZOMBIE		3	/* Task is terminated */
#define TASK_STOPPED		4	/* Task is stopped */
#define TASK_SUSPENDED		5	/* Task is suspended (waiting for IPC) */

/* Special task IDs */
#define TASK_ID_KERNEL		0	/* Kernel task */
#define TASK_ID_INIT		1	/* Init task */
#define TASK_ID_IDLE		2	/* Idle task */
#define TASK_ID_FIRST_USER	3	/* First user task */
#define TASK_ID_MAX		65535	/* Maximum task ID */

/*=============================================================================
 * Memory Object Constants
 *============================================================================*/

/* Memory object types (extending I_TYPE) */
#define MEM_OBJECT_TYPE_MASK	0xFF000000
#define MEM_OBJECT_FILE		I_REGULAR	/* Regular file (compatibility) */
#define MEM_OBJECT_DIR		I_DIRECTORY	/* Directory (compatibility) */
#define MEM_OBJECT_ANON		0x01000000	/* Anonymous memory */
#define MEM_OBJECT_SHARED	0x02000000	/* Shared memory */
#define MEM_OBJECT_MMIO		0x03000000	/* Memory-mapped I/O */
#define MEM_OBJECT_IPC		0x04000000	/* IPC buffer */

/* Memory protection flags (extending original) */
#define MEM_PROT_NONE		0x00	/* No access */
#define MEM_PROT_READ		0x01	/* Read access */
#define MEM_PROT_WRITE		0x02	/* Write access */
#define MEM_PROT_EXEC		0x04	/* Execute access */
#define MEM_PROT_COW		0x08	/* Copy-on-write */
#define MEM_PROT_SHARED		0x10	/* Shared mapping */
#define MEM_PROT_GUARD		0x20	/* Guard page */

/*=============================================================================
 * File System Constants
 *============================================================================*/

/* File types (extending I_TYPE) */
#define FILE_TYPE_MASK		I_TYPE
#define FILE_REGULAR		I_REGULAR
#define FILE_DIRECTORY		I_DIRECTORY
#define FILE_BLOCK_DEV		I_BLOCK_SPECIAL
#define FILE_CHAR_DEV		I_CHAR_SPECIAL
#define FILE_FIFO		I_NAMED_PIPE
#define FILE_SYMLINK		0x0120000	/* Symbolic link (new) */
#define FILE_SOCKET		0x0130000	/* Socket (new) */

/* Maximum path components */
#define MAX_PATH_DEPTH		32	/* Maximum directory depth */
#define MAX_PATH_COMPONENT	256	/* Maximum component length */
#define MAX_PATH_LEN		4096	/* Maximum path length */

/*=============================================================================
 * System Limits
 *============================================================================*/

/* Resource limits */
#define MAX_OPEN_FILES		256	/* Maximum open files per task */
#define MAX_ARG_PAGES		32	/* Maximum argument pages */
#define MAX_ARG_STRLEN		131072	/* Maximum argument string length */
#define MAX_ENVIRON_STRLEN	131072	/* Maximum environment string length */

/* Signal limits */
#define MAX_SIGNAL_QUEUE	32	/* Maximum queued signals per task */
#define MAX_SIGNAL_HANDLERS	32	/* Maximum signal handlers */

/* Timer limits */
#define MAX_TIMERS_PER_TASK	64	/* Maximum timers per task */
#define MAX_TIMER_RESOLUTION	1000	/* Timer resolution (Hz) */

/*=============================================================================
 * Error Constants
 *============================================================================*/

/* Error classes (for server error codes) */
#define ERR_CLASS_SUCCESS	0x0000	/* Success */
#define ERR_CLASS_POSIX		0x0100	/* POSIX error (1-39) */
#define ERR_CLASS_CAP		0x0200	/* Capability error (100-119) */
#define ERR_CLASS_IPC		0x0300	/* IPC error (120-139) */
#define ERR_CLASS_SERVER	0x0400	/* Server error (140-159) */
#define ERR_CLASS_MEM		0x0500	/* Memory error (160-179) */
#define ERR_CLASS_PROC		0x0600	/* Process error (180-199) */
#define ERR_CLASS_FILE		0x0700	/* File error (200-219) */
#define ERR_CLASS_DEV		0x0800	/* Device error (220-239) */
#define ERR_CLASS_SIG		0x0900	/* Signal error (240-259) */
#define ERR_CLASS_TTY		0x0A00	/* Terminal error (260-279) */
#define ERR_CLASS_NET		0x0B00	/* Network error (280-299) */

/*=============================================================================
 * Time Constants
 *============================================================================*/

#define HZ			100	/* Timer interrupt frequency */
#define CLOCK_TICK_RATE		1193180	/* 8253 clock tick rate */
#define CLOCKS_PER_SEC		100	/* POSIX clocks per second */

/* Time conversion macros */
#define MSEC_PER_SEC		1000
#define USEC_PER_SEC		1000000
#define NSEC_PER_SEC		1000000000

/*=============================================================================
 * Bootstrap Constants
 *============================================================================*/

/* Bootstrap stages */
#define BOOT_STAGE_1		1	/* Basic memory initialization */
#define BOOT_STAGE_2		2	/* Server startup */
#define BOOT_STAGE_3		3	/* Init task creation */
#define BOOT_STAGE_4		4	/* User mode transition */
#define BOOT_COMPLETE		5	/* System ready */

/*=============================================================================
 * Debug and Trace Constants
 *============================================================================*/

#define DEBUG_NONE		0x0000	/* No debugging */
#define DEBUG_IPC		0x0001	/* Trace IPC messages */
#define DEBUG_CAP		0x0002	/* Trace capability operations */
#define DEBUG_MEM		0x0004	/* Trace memory operations */
#define DEBUG_PROC		0x0008	/* Trace process operations */
#define DEBUG_FILE		0x0010	/* Trace file operations */
#define DEBUG_ALL		0xFFFF	/* Trace everything */

/*=============================================================================
 * Compile-time assertions for constant sanity
 *============================================================================*/

/* Verify original constants are within expected ranges */
#if BUFFER_END < 0x100000
#error "BUFFER_END too small"
#endif

#if (I_TYPE & I_REGULAR) == 0
#error "I_TYPE mask broken"
#endif

/* Verify new constants don't conflict with old ones */
#if (MEM_OBJECT_ANON & I_TYPE) != 0
#error "Memory object type conflicts with inode type"
#endif

#endif /* _CONST_H */
