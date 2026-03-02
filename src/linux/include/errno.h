/*
* HISTORY
* $Log: errno.h,v $
* Revision 1.1 2026/02/14 07:15:30 pedro
* Microkernel version of error numbers.
* Original errno values preserved for compatibility.
* Added capability-specific error codes.
* errno is now per-task and managed by the kernel state.
* [2026/02/14 pedro]
*/

/*
* File: errno.h
* Author: Linus Torvalds (original Linux version)
*         Pedro Emanuel (microkernel adaptation)
* Date: 2026/02/14
*
* System error numbers for microkernel architecture.
*
* Original Linux 0.11: Simple error codes from Minix/POSIX.
* Microkernel version: Same error numbers preserved for compatibility,
* but now errors can come from multiple servers via IPC. The errno
* variable is maintained per-task in the kernel state.
*
* New capability-specific errors:
* - ECAP: General capability error
* - ECAPNOSPC: No capability space
* - ECAPINV: Invalid capability
* - ECAPREVOKED: Capability revoked
*
* IPC-specific errors:
* - EIPC: General IPC error
* - EIPCNOREPLY: No reply from server
* - EIPCTIMEOUT: IPC timeout
* - EIPCPORT: Invalid port
*/

#ifndef _ERRNO_H
#define _ERRNO_H

typedef int kern_return_t

/*
 * Original note from Linus (preserved for historical context):
 *
 * ok, as I hadn't got any other source of information about
 * possible error numbers, I was forced to use the same numbers
 * as minix.
 * Hopefully these are posix or something. I wouldn't know (and posix
 * isn't telling me - they want $$$ for their f***ing standard).
 *
 * We don't use the _SIGN cludge of minix, so kernel returns must
 * see to the sign by themselves.
 *
 * NOTE! Remember to change strerror() if you change this file!
 */

/* Standard POSIX error numbers (preserved exactly) */
#define EPERM		 1	/* Operation not permitted */
#define ENOENT		 2	/* No such file or directory */
#define ESRCH		 3	/* No such process */
#define EINTR		 4	/* Interrupted system call */
#define EIO		 5	/* I/O error */
#define ENXIO		 6	/* No such device or address */
#define E2BIG		 7	/* Argument list too long */
#define ENOEXEC		 8	/* Exec format error */
#define EBADF		 9	/* Bad file number */
#define ECHILD		10	/* No child processes */
#define EAGAIN		11	/* Try again */
#define ENOMEM		12	/* Out of memory */
#define EACCES		13	/* Permission denied */
#define EFAULT		14	/* Bad address */
#define ENOTBLK		15	/* Block device required */
#define EBUSY		16	/* Device or resource busy */
#define EEXIST		17	/* File exists */
#define EXDEV		18	/* Cross-device link */
#define ENODEV		19	/* No such device */
#define ENOTDIR		20	/* Not a directory */
#define EISDIR		21	/* Is a directory */
#define EINVAL		22	/* Invalid argument */
#define ENFILE		23	/* File table overflow */
#define EMFILE		24	/* Too many open files */
#define ENOTTY		25	/* Not a typewriter */
#define ETXTBSY		26	/* Text file busy */
#define EFBIG		27	/* File too large */
#define ENOSPC		28	/* No space left on device */
#define ESPIPE		29	/* Illegal seek */
#define EROFS		30	/* Read-only file system */
#define EMLINK		31	/* Too many links */
#define EPIPE		32	/* Broken pipe */
#define EDOM		33	/* Math argument out of domain of func */
#define ERANGE		34	/* Math result not representable */
#define EDEADLK		35	/* Resource deadlock would occur */
#define ENAMETOOLONG	36	/* File name too long */
#define ENOLCK		37	/* No record locks available */
#define ENOSYS		38	/* Function not implemented */
#define ENOTEMPTY	39	/* Directory not empty */

/* Original Linux 0.11 also had this generic error */
#define ERROR		99	/* Generic error */

/*
 * Microkernel-specific error numbers
 * These extend the original range (starting at 100)
 */

/* Capability-related errors (100-119) */
#define ECAP		100	/* Generic capability error */
#define ECAPNOSPC	101	/* No capability space available */
#define ECAPINV		102	/* Invalid capability */
#define ECAPREVOKED	103	/* Capability revoked */
#define ECAPRIGHTS	104	/* Insufficient capability rights */
#define ECAPDOMAIN	105	/* Invalid capability domain */
#define ECAPBOUND	106	/* Capability bounds exceeded */
#define ECAPNOTASK	107	/* Not a task capability */
#define ECAPNOTPORT	108	/* Not a port capability */
#define ECAPNOTMEM	109	/* Not a memory capability */
#define ECAPNOTDEV	110	/* Not a device capability */

/* IPC-related errors (120-139) */
#define EIPC		120	/* Generic IPC error */
#define EIPCNOREPLY	121	/* No reply from server */
#define EIPCTIMEOUT	122	/* IPC timeout */
#define EIPCPORT	123	/* Invalid IPC port */
#define EIPCQUEUE	124	/* IPC queue full */
#define EIPCMSG		125	/* Invalid IPC message */
#define EIPCSIZE	126	/* IPC message size error */
#define EIPCDEST	127	/* Destination port not found */
#define EIPCSRC		128	/* Source port not found */
#define EIPCACCESS	129	/* IPC access denied */

/* Server-related errors (140-159) */
#define ESERVER		140	/* Generic server error */
#define ESERVERDOWN	141	/* Server not responding */
#define ESERVERBUSY	142	/* Server busy */
#define ESERVERCONF	143	/* Server configuration error */
#define ESERVERAUTH	144	/* Server authentication failed */

/* Memory server errors (160-179) */
#define EMEM		160	/* Generic memory error */
#define EMEMPAGE	161	/* Page allocation failed */
#define EMEMREGION	162	/* Region allocation failed */
#define EMEMPROT	163	/* Protection error */
#define EMEMSHARE	164	/* Sharing error */
#define EMEMCOW		165	/* Copy-on-write error */
#define EMEMOBJECT	166	/* Memory object error */

/* Process server errors (180-199) */
#define EPROC		180	/* Generic process error */
#define EPROCTASK	181	/* Task creation failed */
#define EPROCSLOT	182	/* No task slot available */
#define EPROCPRIO	183	/* Invalid priority */
#define EPROCSCHED	184	/* Scheduling error */

/* File server errors (200-219) */
#define EFILE		200	/* Generic file error */
#define EFILEOPEN	201	/* File open failed */
#define EFILECLOSE	202	/* File close failed */
#define EFILEREAD	203	/* File read failed */
#define EFILEWRITE	204	/* File write failed */
#define EFILESEEK	205	/* File seek failed */
#define EFILELOCK	206	/* File lock failed */

/* Device server errors (220-239) */
#define EDEV		220	/* Generic device error */
#define EDEVIO		221	/* Device I/O error */
#define EDEVNOTREADY	222	/* Device not ready */
#define EDEVNOINTR	223	/* Device interrupt not available */
#define EDEVDMA		224	/* DMA error */

/* Signal server errors (240-259) */
#define ESIG		240	/* Generic signal error */
#define ESIGHANDLER	241	/* Signal handler error */
#define ESIGQUEUE	242	/* Signal queue full */
#define ESIGMASK	243	/* Signal mask error */

/* Terminal server errors (260-279) */
#define ETTY		260	/* Generic terminal error */
#define ETTYLINE	261	/* Line discipline error */
#define ETTYBAUD	262	/* Invalid baud rate */
#define ETTYMODE	263	/* Invalid terminal mode */

/* Network errors (if applicable) (280-299) */
#define ENET		280	/* Generic network error */
#define ENETDOWN	281	/* Network is down */
#define ENETUNREACH	282	/* Network unreachable */
#define ENETRESET	283	/* Network reset */

/* System server errors (300-319) */
#define ESYS		300	/* Generic system error */
#define ESYSCONF	301	/* System configuration error */
#define ESYSTIME	302	/* Time server error */
#define ESYSBOOT	303	/* Boot error */

/* Resource limits */
#define ELIMIT		320	/* Resource limit exceeded */
#define ELIMITCPU	321	/* CPU time limit exceeded */
#define ELIMITFILE	322	/* File size limit exceeded */
#define ELIMITMEM	323	/* Memory limit exceeded */
#define ELIMITPROC	324	/* Process limit exceeded */

/*
 * errno - per-task error number
 *
 * In microkernel mode, errno is stored in the kernel state
 * for each task. This macro provides access to the current
 * task's errno.
 */
extern int errno;

/* For internal kernel use - access to current task's errno */
#define current_errno (kernel_state->tasks[kernel_state->current_task].errno)

/* Helper macros for error handling */
#define IS_ERROR(val) ((val) < 0)
#define ERROR_CODE(val) (-(val))
#define RETURN_ERROR(err) do { errno = (err); return -1; } while(0)
#define RETURN_CODE(err, val) do { errno = (err); return (val); } while(0)

/* Error checking macros for IPC */
#define CHECK_IPC_RESULT(result) \
	do { if ((result) < 0) { errno = EIPC; return -1; } } while(0)

#define CHECK_CAPABILITY(cap) \
	do { if (!((current_capability) & (cap))) { errno = ECAPRIGHTS; return -1; } } while(0)

/* Convert server error codes to errno */
static inline int server_error_to_errno(int server_error)
{
	/* Server errors are negative */
	if (server_error >= 0)
		return 0;
	
	/* Direct mapping for POSIX errors */
	switch (-server_error) {
		case 1: return EPERM;
		case 2: return ENOENT;
		case 3: return ESRCH;
		case 4: return EINTR;
		case 5: return EIO;
		case 6: return ENXIO;
		case 7: return E2BIG;
		case 8: return ENOEXEC;
		case 9: return EBADF;
		case 10: return ECHILD;
		case 11: return EAGAIN;
		case 12: return ENOMEM;
		case 13: return EACCES;
		case 14: return EFAULT;
		case 15: return ENOTBLK;
		case 16: return EBUSY;
		case 17: return EEXIST;
		case 18: return EXDEV;
		case 19: return ENODEV;
		case 20: return ENOTDIR;
		case 21: return EISDIR;
		case 22: return EINVAL;
		case 23: return ENFILE;
		case 24: return EMFILE;
		case 25: return ENOTTY;
		case 26: return ETXTBSY;
		case 27: return EFBIG;
		case 28: return ENOSPC;
		case 29: return ESPIPE;
		case 30: return EROFS;
		case 31: return EMLINK;
		case 32: return EPIPE;
		case 33: return EDOM;
		case 34: return ERANGE;
		case 35: return EDEADLK;
		case 36: return ENAMETOOLONG;
		case 37: return ENOLCK;
		case 38: return ENOSYS;
		case 39: return ENOTEMPTY;
		case 99: return ERROR;
		
		/* Capability errors */
		case 100: return ECAP;
		case 101: return ECAPNOSPC;
		case 102: return ECAPINV;
		case 103: return ECAPREVOKED;
		case 104: return ECAPRIGHTS;
		
		/* IPC errors */
		case 120: return EIPC;
		case 121: return EIPCNOREPLY;
		case 122: return EIPCTIMEOUT;
		case 123: return EIPCPORT;
		
		default: return ERROR;
	}
}

/* Convert errno to server error code */
static inline int errno_to_server_error(int err)
{
	if (err == 0)
		return 0;
	return -err;
}

/* Error description strings (for strerror) */
static inline const char *strerror(int errnum)
{
	static const char * const error_strings[] = {
		[0]			= "Success",
		[EPERM]			= "Operation not permitted",
		[ENOENT]		= "No such file or directory",
		[ESRCH]			= "No such process",
		[EINTR]			= "Interrupted system call",
		[EIO]			= "I/O error",
		[ENXIO]			= "No such device or address",
		[E2BIG]			= "Argument list too long",
		[ENOEXEC]		= "Exec format error",
		[EBADF]			= "Bad file number",
		[ECHILD]		= "No child processes",
		[EAGAIN]		= "Try again",
		[ENOMEM]		= "Out of memory",
		[EACCES]		= "Permission denied",
		[EFAULT]		= "Bad address",
		[ENOTBLK]		= "Block device required",
		[EBUSY]			= "Device or resource busy",
		[EEXIST]		= "File exists",
		[EXDEV]			= "Cross-device link",
		[ENODEV]		= "No such device",
		[ENOTDIR]		= "Not a directory",
		[EISDIR]		= "Is a directory",
		[EINVAL]		= "Invalid argument",
		[ENFILE]		= "File table overflow",
		[EMFILE]		= "Too many open files",
		[ENOTTY]		= "Not a typewriter",
		[ETXTBSY]		= "Text file busy",
		[EFBIG]			= "File too large",
		[ENOSPC]		= "No space left on device",
		[ESPIPE]		= "Illegal seek",
		[EROFS]			= "Read-only file system",
		[EMLINK]		= "Too many links",
		[EPIPE]			= "Broken pipe",
		[EDOM]			= "Math argument out of domain",
		[ERANGE]		= "Math result not representable",
		[EDEADLK]		= "Resource deadlock would occur",
		[ENAMETOOLONG]		= "File name too long",
		[ENOLCK]		= "No record locks available",
		[ENOSYS]		= "Function not implemented",
		[ENOTEMPTY]		= "Directory not empty",
		[ERROR]			= "Generic error",
		
		/* Capability errors */
		[ECAP]			= "Generic capability error",
		[ECAPNOSPC]		= "No capability space available",
		[ECAPINV]		= "Invalid capability",
		[ECAPREVOKED]		= "Capability revoked",
		[ECAPRIGHTS]		= "Insufficient capability rights",
		
		/* IPC errors */
		[EIPC]			= "Generic IPC error",
		[EIPCNOREPLY]		= "No reply from server",
		[EIPCTIMEOUT]		= "IPC timeout",
		[EIPCPORT]		= "Invalid IPC port",
	};
	
	if (errnum >= 0 && errnum < sizeof(error_strings)/sizeof(char *))
		return error_strings[errnum] ? error_strings[errnum] : "Unknown error";
	return "Unknown error";
}

/* Perror - print error message */
static inline void perror(const char *s)
{
	if (s && *s)
		printf("%s: %s\n", s, strerror(errno));
	else
		printf("%s\n", strerror(errno));
}

#endif /* _ERRNO_H */
