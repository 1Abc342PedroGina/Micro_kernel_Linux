/*
* HISTORY
* $Log: types.h,v $
* Revision 1.1 2026/02/13 23:15:30 pedro
* Microkernel version of system types.
* Basic types preserved for compatibility.
* Added capability-specific types.
* uid_t/gid_t now represent capability domains.
* dev_t represents device server ports.
* [2026/02/13 pedro]
*/

/*
* File: sys/types.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/13
*
* System types for microkernel architecture.
*
* Original Linux 0.11: Basic POSIX types for Unix compatibility.
* Microkernel version: Same types preserved for compatibility,
* but their meaning now reflects microkernel concepts:
*
* - pid_t: Task capability ID (not just process number)
* - uid_t/gid_t: Capability domain IDs (not just user/group numbers)
* - dev_t: Device server port number
* - ino_t: Memory object capability ID
* - mode_t: Access mode flags (now combined with capability rights)
*/

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

/*
 * Size type - unchanged, but now represents bytes in capability-aware
 * memory operations. The memory server validates that the size doesn't
 * exceed the task's capability bounds.
 */
#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

/*
 * Time type - now handled by time server via IPC.
 * The time_t value is obtained from the time server,
 * which maintains system time with proper capabilities.
 */
#ifndef _TIME_T
#define _TIME_T
typedef long time_t;
#endif

/*
 * Pointer difference type - still valid as long as both pointers
 * are in the same capability space. Cross-space pointer arithmetic
 * is forbidden and will cause capability violations.
 */
#ifndef _PTRDIFF_T
#define _PTRDIFF_T
typedef long ptrdiff_t;
#endif

/*
 * NULL pointer - unchanged, but now represents "no capability"
 * in capability contexts. The memory server treats NULL as
 * an invalid capability reference.
 */
#ifndef NULL
#define NULL ((void *) 0)
#endif

/*
 * Process ID type - now represents a task capability.
 * Not just a number, but a capability that grants rights
 * to control the task (signal, wait, etc.).
 *
 * Range: 1-65535 (0 is reserved for kernel capability)
 */
typedef int pid_t;           /* Task capability ID */

/*
 * User ID type - now represents a capability domain.
 * In microkernel, uids are not just authentication,
 * but define which capabilities a task can acquire.
 *
 * Example domains:
 * - 0: Kernel/capability domain (CAP_ALL)
 * - 1: System domain (CAP_SYSTEM)
 * - 2: User domain (basic capabilities)
 * - 3: Guest domain (restricted)
 */
typedef unsigned short uid_t;  /* Capability domain ID */

/*
 * Group ID type - secondary capability domain.
 * Used for capability inheritance and sharing.
 */
typedef unsigned char gid_t;   /* Secondary domain ID */

/*
 * Device ID type - now represents device server port.
 * Each device (console, disk, tty) has a unique port
 * that tasks use to send I/O requests.
 */
typedef unsigned short dev_t;  /* Device server port */

/*
 * Inode number type - now represents memory object capability.
 * Files and memory objects are unified in microkernel -
 * both are "memory objects" accessed via capabilities.
 */
typedef unsigned short ino_t;  /* Memory object capability ID */

/*
 * File mode type - now combined with capability rights.
 * The lower bits are POSIX permissions (rwx),
 * upper bits are capability flags:
 * - 0x8000: CAP_COPY (copy-on-write)
 * - 0x4000: CAP_SHARE (shared access)
 * - 0x2000: CAP_IPC (IPC capability)
 */
typedef unsigned short mode_t;  /* Mode + capability flags */
typedef unsigned short umode_t; /* User mode (same) */

/*
 * Link count type - now represents number of capability
 * references to an object (for garbage collection).
 */
typedef unsigned char nlink_t;  /* Capability reference count */

/*
 * Disk address type - now represents offset in memory object.
 * Used by file server to access disk via device server.
 */
typedef int daddr_t;  /* Memory object offset */

/*
 * File offset type - now represents position in memory object.
 * The memory server validates that offset+count stays within
 * the object's capability bounds.
 */
typedef long off_t;   /* Offset in memory object */

/*
 * Unsigned character type - unchanged, but when used in
 * cross-space operations (get_fs_byte), it triggers capability
 * validation by the memory server.
 */
typedef unsigned char u_char;

/*
 * Unsigned short type - unchanged.
 */
typedef unsigned short ushort;

/*
 * Division type for div() - unchanged.
 */
typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;

/*
 * Ustat structure - for file system statistics.
 * In microkernel, this is obtained from the file server
 * via IPC, not directly from disk.
 */
struct ustat {
	daddr_t f_tfree;     /* Free blocks (in memory objects) */
	ino_t f_tinode;      /* Free inodes (capabilities) */
	char f_fname[6];     /* File system name */
	char f_fpack[6];     /* Pack name */
};

/*
 * Microkernel-specific capability types (extensions)
 * These are not in original Linux 0.11 but are needed
 * for the microkernel architecture.
 */

#ifndef _CAPABILITY_T
#define _CAPABILITY_T
typedef unsigned int capability_t;  /* Capability mask */
#endif

#ifndef _PORT_T
#define _PORT_T
typedef unsigned int port_t;        /* IPC port ID */
#endif

#ifndef _SPACE_T
#define _SPACE_T
typedef unsigned int space_t;        /* Capability space ID */
#endif

#ifndef _OBJECT_T
#define _OBJECT_T
typedef unsigned int object_t;       /* Memory object ID */
#endif

#ifndef _TASK_T
#define _TASK_T
typedef pid_t task_t;                 /* Task capability (same as pid_t) */
#endif

/* Capability rights masks (for use with mode_t) */
#define CAP_RIGHTS_MASK   0xFF00      /* Upper byte for capability rights */
#define CAP_READ          0x0100      /* Read capability */
#define CAP_WRITE         0x0200      /* Write capability */
#define CAP_EXEC          0x0400      /* Execute capability */
#define CAP_SHARE         0x0800      /* Share capability */
#define CAP_COPY          0x1000      /* Copy-on-write capability */
#define CAP_IPC           0x2000      /* IPC capability */

/* Special capability values */
#define CAP_KERNEL        0x0000      /* Kernel capability space */
#define CAP_NULL          0xFFFF      /* Null capability (no rights) */

/* Convert between POSIX mode and capability rights */
#define MODE_TO_CAPS(mode) (((mode) & 0700) ? CAP_READ|CAP_WRITE|CAP_EXEC : 0)
#define CAPS_TO_MODE(caps) (((caps) & CAP_READ) ? 0400 : 0) | \
                           (((caps) & CAP_WRITE) ? 0200 : 0) | \
                           (((caps) & CAP_EXEC) ? 0100 : 0)

/*
 * Task capability validation macro
 * Checks if a pid_t is a valid task capability
 */
#define IS_VALID_TASK(pid) ((pid) > 0 && (pid) < 65536)

/*
 * Port validation macro
 * Checks if a port_t is valid
 */
#define IS_VALID_PORT(port) ((port) >= 0x0001 && (port) <= 0xFFFE)

/*
 * Capability space validation
 */
#define IS_VALID_SPACE(space) ((space) >= 0 && (space) < 16)

/*
 * Memory object validation
 */
#define IS_VALID_OBJECT(obj) ((obj) != 0)

/*
 * Special port numbers (reserved)
 */
#define PORT_KERNEL       0x0001  /* Kernel port */
#define PORT_BOOTSTRAP    0x0002  /* Bootstrap port */
#define PORT_MEMORY       0x0003  /* Memory server port */
#define PORT_PROCESS      0x0004  /* Process server port */
#define PORT_DEVICE       0x0005  /* Device server port */
#define PORT_CONSOLE      0x0006  /* Console server port */
#define PORT_SYSTEM       0x0007  /* System server port */

/*
 * Special task IDs
 */
#define TASK_KERNEL       0       /* Kernel task */
#define TASK_INIT         1       /* Init task */
#define TASK_IDLE         2       /* Idle task */

/*
 * Capability domain IDs (for uid_t)
 */
#define DOMAIN_KERNEL     0       /* Kernel domain (CAP_ALL) */
#define DOMAIN_SYSTEM     1       /* System domain (CAP_SYSTEM) */
#define DOMAIN_ROOT       2       /* Root domain (CAP_ROOT) */
#define DOMAIN_USER       3       /* User domain (basic caps) */
#define DOMAIN_GUEST      4       /* Guest domain (restricted) */

#endif /* _SYS_TYPES_H */
